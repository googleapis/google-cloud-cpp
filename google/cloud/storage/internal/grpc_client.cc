// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/internal/grpc_configure_client_context.h"
#include "google/cloud/storage/internal/grpc_object_read_source.h"
#include "google/cloud/storage/internal/grpc_resumable_upload_session.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/internal/resumable_upload_session.h"
#include "google/cloud/storage/internal/sha256_hash.h"
#include "google/cloud/storage/internal/storage_auth.h"
#include "google/cloud/storage/internal/storage_round_robin.h"
#include "google/cloud/storage/internal/storage_stub.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/big_endian.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/log.h"
#include "absl/strings/str_split.h"
#include "absl/time/time.h"
#include <crc32c/crc32c.h>
#include <grpcpp/grpcpp.h>
#include <algorithm>
#include <cinttypes>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

using ::google::cloud::internal::GrpcAuthenticationStrategy;
using ::google::cloud::internal::MakeBackgroundThreadsFactory;

auto constexpr kDirectPathConfig = R"json({
    "loadBalancingConfig": [{
      "grpclb": {
        "childPolicy": [{
          "pick_first": {}
        }]
      }
    }]
  })json";

int DefaultGrpcNumChannels() {
  auto constexpr kMinimumChannels = 4;
  auto const count = std::thread::hardware_concurrency();
  return (std::max)(kMinimumChannels, static_cast<int>(count));
}

Options DefaultOptionsGrpc(Options options) {
  options = DefaultOptionsWithCredentials(std::move(options));
  if (!options.has<UnifiedCredentialsOption>() &&
      !options.has<GrpcCredentialOption>()) {
    options.set<UnifiedCredentialsOption>(
        google::cloud::MakeGoogleDefaultCredentials());
  }
  if (!options.has<EndpointOption>()) {
    options.set<EndpointOption>("storage.googleapis.com");
  }
  auto env = google::cloud::internal::GetEnv("CLOUD_STORAGE_GRPC_ENDPOINT");
  if (env.has_value()) {
    options.set<UnifiedCredentialsOption>(MakeInsecureCredentials());
    options.set<EndpointOption>(*env);
  }
  if (!options.has<GrpcNumChannelsOption>()) {
    options.set<GrpcNumChannelsOption>(DefaultGrpcNumChannels());
  }
  return options;
}

std::shared_ptr<grpc::Channel> CreateGrpcChannel(
    GrpcAuthenticationStrategy& auth, Options const& options, int channel_id) {
  grpc::ChannelArguments args;
  auto const& config = options.get<storage_experimental::GrpcPluginOption>();
  if (config.empty() || config == "default" || config == "none") {
    // Just configure for the regular path.
    args.SetInt("grpc.channel_id", channel_id);
    return auth.CreateChannel(options.get<EndpointOption>(), std::move(args));
  }
  std::set<absl::string_view> settings = absl::StrSplit(config, ',');
  auto const dp = settings.count("dp") != 0 || settings.count("alts") != 0;
  if (dp || settings.count("pick-first-lb") != 0) {
    args.SetServiceConfigJSON(kDirectPathConfig);
  }
  if (dp || settings.count("enable-dns-srv-queries") != 0) {
    args.SetInt("grpc.dns_enable_srv_queries", 1);
  }
  if (settings.count("disable-dns-srv-queries") != 0) {
    args.SetInt("grpc.dns_enable_srv_queries", 0);
  }
  if (settings.count("exclusive") != 0) {
    args.SetInt("grpc.channel_id", channel_id);
  }
  if (settings.count("alts") != 0) {
    grpc::experimental::AltsCredentialsOptions alts_opts;
    return grpc::CreateCustomChannel(
        options.get<EndpointOption>(),
        grpc::CompositeChannelCredentials(
            grpc::experimental::AltsCredentials(alts_opts),
            grpc::GoogleComputeEngineCredentials()),
        std::move(args));
  }
  return auth.CreateChannel(options.get<EndpointOption>(), std::move(args));
}

std::shared_ptr<StorageStub> CreateStorageStub(CompletionQueue cq,
                                               Options const& opts) {
  auto auth = google::cloud::internal::CreateAuthenticationStrategy(
      std::move(cq), opts);
  std::vector<std::shared_ptr<StorageStub>> children(
      (std::max)(1, opts.get<GrpcNumChannelsOption>()));
  int id = 0;
  std::generate(children.begin(), children.end(), [&id, &auth, opts] {
    return MakeDefaultStorageStub(CreateGrpcChannel(*auth, opts, id++));
  });
  std::shared_ptr<StorageStub> stub =
      std::make_shared<StorageRoundRobin>(std::move(children));
  if (auth->RequiresConfigureContext()) {
    stub = std::make_shared<StorageAuth>(std::move(auth), std::move(stub));
  }
  return stub;
}

std::shared_ptr<GrpcClient> GrpcClient::Create(Options opts) {
  // Cannot use std::make_shared<> as the constructor is private.
  return std::shared_ptr<GrpcClient>(new GrpcClient(std::move(opts)));
}

std::shared_ptr<GrpcClient> GrpcClient::CreateMock(
    std::shared_ptr<StorageStub> stub, Options opts) {
  return std::shared_ptr<GrpcClient>(
      new GrpcClient(std::move(stub), DefaultOptionsGrpc(std::move(opts))));
}

GrpcClient::GrpcClient(Options opts)
    : options_(std::move(opts)),
      backwards_compatibility_options_(
          MakeBackwardsCompatibleClientOptions(options_)),
      background_(MakeBackgroundThreadsFactory(options_)()),
      stub_(CreateStorageStub(background_->cq(), options_)) {}

GrpcClient::GrpcClient(std::shared_ptr<StorageStub> stub, Options opts)
    : options_(std::move(opts)),
      backwards_compatibility_options_(
          MakeBackwardsCompatibleClientOptions(options_)),
      background_(MakeBackgroundThreadsFactory(options_)()),
      stub_(std::move(stub)) {}

std::unique_ptr<GrpcClient::WriteObjectStream> GrpcClient::CreateUploadWriter(
    std::unique_ptr<grpc::ClientContext> context) {
  auto const timeout = options_.get<TransferStallTimeoutOption>();
  if (timeout.count() != 0) {
    context->set_deadline(std::chrono::system_clock::now() + timeout);
  }
  return stub_->WriteObject(std::move(context));
}

StatusOr<ResumableUploadResponse> GrpcClient::QueryResumableUpload(
    QueryResumableUploadRequest const& request) {
  grpc::ClientContext context;
  ApplyQueryParameters(context, request, "resource");
  auto const timeout = options_.get<TransferStallTimeoutOption>();
  if (timeout.count() != 0) {
    context.set_deadline(std::chrono::system_clock::now() + timeout);
  }
  auto status = stub_->QueryWriteStatus(context, ToProto(request));
  if (!status) return std::move(status).status();

  ResumableUploadResponse response;
  response.upload_state = ResumableUploadResponse::kInProgress;
  // TODO(#6880) - cleanup the committed_byte vs. size thing
  if (status->has_persisted_size() && status->persisted_size()) {
    response.last_committed_byte =
        static_cast<std::uint64_t>(status->persisted_size());
  } else {
    response.last_committed_byte = 0;
  }
  if (status->has_resource()) {
    response.payload = FromProto(status->resource(), options());
    response.upload_state = ResumableUploadResponse::kDone;
  }
  return response;
}

StatusOr<std::unique_ptr<ResumableUploadSession>>
GrpcClient::FullyRestoreResumableSession(ResumableUploadRequest const& request,
                                         std::string const& upload_url) {
  auto self = shared_from_this();
  auto upload_session_params = DecodeGrpcResumableUploadSessionUrl(upload_url);
  if (!upload_session_params) return std::move(upload_session_params).status();
  auto session = std::unique_ptr<ResumableUploadSession>(
      new GrpcResumableUploadSession(self, request, *upload_session_params));
  auto response = session->ResetSession();
  if (!response) std::move(response).status();
  return session;
}

ClientOptions const& GrpcClient::client_options() const {
  return backwards_compatibility_options_;
}

StatusOr<ListBucketsResponse> GrpcClient::ListBuckets(
    ListBucketsRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<BucketMetadata> GrpcClient::CreateBucket(CreateBucketRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<BucketMetadata> GrpcClient::GetBucketMetadata(
    GetBucketMetadataRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<EmptyResponse> GrpcClient::DeleteBucket(DeleteBucketRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<BucketMetadata> GrpcClient::UpdateBucket(UpdateBucketRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<BucketMetadata> GrpcClient::PatchBucket(PatchBucketRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<IamPolicy> GrpcClient::GetBucketIamPolicy(
    GetBucketIamPolicyRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<NativeIamPolicy> GrpcClient::GetNativeBucketIamPolicy(
    GetBucketIamPolicyRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<IamPolicy> GrpcClient::SetBucketIamPolicy(
    SetBucketIamPolicyRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<NativeIamPolicy> GrpcClient::SetNativeBucketIamPolicy(
    SetNativeBucketIamPolicyRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<TestBucketIamPermissionsResponse> GrpcClient::TestBucketIamPermissions(
    TestBucketIamPermissionsRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<BucketMetadata> GrpcClient::LockBucketRetentionPolicy(
    LockBucketRetentionPolicyRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ObjectMetadata> GrpcClient::InsertObjectMedia(
    InsertObjectMediaRequest const& request) {
  auto r = ToProto(request);
  if (!r) return std::move(r).status();
  auto proto_request = *r;

  auto context = absl::make_unique<grpc::ClientContext>();
  // The REST response is just the object metadata (aka the "resource"). In the
  // gRPC response the object metadata is in a "resource" field. Passing an
  // extra prefix to ApplyQueryParameters sends the right filtering instructions
  // to the gRPC API.
  ApplyQueryParameters(*context, request, "resource");
  auto stream = stub_->WriteObject(std::move(context));

  auto const& contents = request.contents();
  auto const contents_size = static_cast<std::int64_t>(contents.size());
  std::int64_t const maximum_buffer_size =
      google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES;

  // This loop must run at least once because we need to send at least one
  // Write() call for empty objects.
  for (std::int64_t offset = 0, n = 0; offset <= contents_size; offset += n) {
    proto_request.set_write_offset(offset);
    auto& data = *proto_request.mutable_checksummed_data();
    n = (std::min)(contents_size - offset, maximum_buffer_size);
    data.set_content(
        contents.substr(static_cast<std::string::size_type>(offset),
                        static_cast<std::string::size_type>(n)));
    data.set_crc32c(crc32c::Crc32c(data.content()));

    if (offset + n >= contents_size) {
      proto_request.set_finish_write(true);
      stream->Write(proto_request, grpc::WriteOptions{}.set_last_message());
      break;
    }
    if (!stream->Write(proto_request, grpc::WriteOptions{})) break;
    // After the first message, clear the object specification and checksums,
    // there is no need to resend it.
    proto_request.clear_write_object_spec();
    proto_request.clear_object_checksums();
  }

  auto response = stream->Close();
  if (!response) return std::move(response).status();
  if (response->has_resource()) {
    return FromProto(response->resource(), options());
  }
  return ObjectMetadata{};
}

StatusOr<ObjectMetadata> GrpcClient::CopyObject(CopyObjectRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ObjectMetadata> GrpcClient::GetObjectMetadata(
    GetObjectMetadataRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<std::unique_ptr<ObjectReadSource>> GrpcClient::ReadObject(
    ReadObjectRangeRequest const& request) {
  // With the REST API this condition was detected by the server as an error,
  // generally we prefer the server to detect errors because its answers are
  // authoritative. In this case, the server cannot: with gRPC '0' is the same
  // as "not set" and the server would send back the full file, which was
  // unlikely to be the customer's intent.
  if (request.HasOption<ReadLast>() &&
      request.GetOption<ReadLast>().value() == 0) {
    return Status(
        StatusCode::kOutOfRange,
        "ReadLast(0) is invalid in REST and produces incorrect output in gRPC");
  }
  auto context = absl::make_unique<grpc::ClientContext>();
  ApplyQueryParameters(*context, request);
  auto const timeout = options_.get<TransferStallTimeoutOption>();
  if (timeout.count() != 0) {
    context->set_deadline(std::chrono::system_clock::now() + timeout);
  }
  auto proto_request = ToProto(request);
  if (!proto_request) return std::move(proto_request).status();
  return std::unique_ptr<ObjectReadSource>(
      absl::make_unique<GrpcObjectReadSource>(
          stub_->ReadObject(std::move(context), *proto_request)));
}

StatusOr<ListObjectsResponse> GrpcClient::ListObjects(
    ListObjectsRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<EmptyResponse> GrpcClient::DeleteObject(DeleteObjectRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ObjectMetadata> GrpcClient::UpdateObject(UpdateObjectRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ObjectMetadata> GrpcClient::PatchObject(PatchObjectRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ObjectMetadata> GrpcClient::ComposeObject(
    ComposeObjectRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<RewriteObjectResponse> GrpcClient::RewriteObject(
    RewriteObjectRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<std::unique_ptr<ResumableUploadSession>>
GrpcClient::CreateResumableSession(ResumableUploadRequest const& request) {
  auto session_id = request.GetOption<UseResumableUploadSession>().value_or("");
  if (!session_id.empty()) {
    return FullyRestoreResumableSession(request, session_id);
  }

  auto proto_request = ToProto(request);
  if (!proto_request) return std::move(proto_request).status();

  grpc::ClientContext context;
  ApplyQueryParameters(context, request, "resource");
  auto const timeout = options_.get<TransferStallTimeoutOption>();
  if (timeout.count() != 0) {
    context.set_deadline(std::chrono::system_clock::now() + timeout);
  }
  auto response = stub_->StartResumableWrite(context, *proto_request);
  if (!response.ok()) return std::move(response).status();

  auto self = shared_from_this();
  return std::unique_ptr<ResumableUploadSession>(
      new GrpcResumableUploadSession(self, request, {response->upload_id()}));
}

StatusOr<EmptyResponse> GrpcClient::DeleteResumableUpload(
    DeleteResumableUploadRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ListBucketAclResponse> GrpcClient::ListBucketAcl(
    ListBucketAclRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<BucketAccessControl> GrpcClient::GetBucketAcl(
    GetBucketAclRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<BucketAccessControl> GrpcClient::CreateBucketAcl(
    CreateBucketAclRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<EmptyResponse> GrpcClient::DeleteBucketAcl(
    DeleteBucketAclRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ListObjectAclResponse> GrpcClient::ListObjectAcl(
    ListObjectAclRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<BucketAccessControl> GrpcClient::UpdateBucketAcl(
    UpdateBucketAclRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<BucketAccessControl> GrpcClient::PatchBucketAcl(
    PatchBucketAclRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ObjectAccessControl> GrpcClient::CreateObjectAcl(
    CreateObjectAclRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<EmptyResponse> GrpcClient::DeleteObjectAcl(
    DeleteObjectAclRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ObjectAccessControl> GrpcClient::GetObjectAcl(
    GetObjectAclRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ObjectAccessControl> GrpcClient::UpdateObjectAcl(
    UpdateObjectAclRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ObjectAccessControl> GrpcClient::PatchObjectAcl(
    PatchObjectAclRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ListDefaultObjectAclResponse> GrpcClient::ListDefaultObjectAcl(
    ListDefaultObjectAclRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ObjectAccessControl> GrpcClient::CreateDefaultObjectAcl(
    CreateDefaultObjectAclRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<EmptyResponse> GrpcClient::DeleteDefaultObjectAcl(
    DeleteDefaultObjectAclRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ObjectAccessControl> GrpcClient::GetDefaultObjectAcl(
    GetDefaultObjectAclRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ObjectAccessControl> GrpcClient::UpdateDefaultObjectAcl(
    UpdateDefaultObjectAclRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ObjectAccessControl> GrpcClient::PatchDefaultObjectAcl(
    PatchDefaultObjectAclRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ServiceAccount> GrpcClient::GetServiceAccount(
    GetProjectServiceAccountRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ListHmacKeysResponse> GrpcClient::ListHmacKeys(
    ListHmacKeysRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<CreateHmacKeyResponse> GrpcClient::CreateHmacKey(
    CreateHmacKeyRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<EmptyResponse> GrpcClient::DeleteHmacKey(DeleteHmacKeyRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<HmacKeyMetadata> GrpcClient::GetHmacKey(GetHmacKeyRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<HmacKeyMetadata> GrpcClient::UpdateHmacKey(
    UpdateHmacKeyRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<SignBlobResponse> GrpcClient::SignBlob(SignBlobRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ListNotificationsResponse> GrpcClient::ListNotifications(
    ListNotificationsRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<NotificationMetadata> GrpcClient::CreateNotification(
    CreateNotificationRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<NotificationMetadata> GrpcClient::GetNotification(
    GetNotificationRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<EmptyResponse> GrpcClient::DeleteNotification(
    DeleteNotificationRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

template <typename GrpcRequest, typename StorageRequest>
void SetCommonParameters(GrpcRequest& request, StorageRequest const& req) {
  if (req.template HasOption<UserProject>()) {
    request.mutable_common_request_params()->set_user_project(
        req.template GetOption<UserProject>().value());
  }
}

template <typename GrpcRequest, typename StorageRequest>
Status SetCommonObjectParameters(GrpcRequest& request,
                                 StorageRequest const& req) {
  if (req.template HasOption<EncryptionKey>()) {
    auto data = req.template GetOption<EncryptionKey>().value();
    auto key_bytes = Base64Decode(data.key);
    if (!key_bytes) return std::move(key_bytes).status();
    auto key_sha256_bytes = Base64Decode(data.sha256);
    if (!key_sha256_bytes) return std::move(key_sha256_bytes).status();

    request.mutable_common_object_request_params()->set_encryption_algorithm(
        std::move(data.algorithm));
    request.mutable_common_object_request_params()->set_encryption_key_bytes(
        std::string{key_bytes->begin(), key_bytes->end()});
    request.mutable_common_object_request_params()
        ->set_encryption_key_sha256_bytes(
            std::string{key_sha256_bytes->begin(), key_sha256_bytes->end()});
  }
  return Status{};
}

template <typename GrpcRequest>
struct GetPredefinedAcl {
  auto operator()(GrpcRequest const& q) -> decltype(q.predefined_acl());
};

template <
    typename GrpcRequest, typename StorageRequest,
    typename std::enable_if<
        std::is_same<google::storage::v2::PredefinedObjectAcl,
                     google::cloud::internal::invoke_result_t<
                         GetPredefinedAcl<GrpcRequest>, GrpcRequest>>::value,
        int>::type = 0>
void SetPredefinedAcl(GrpcRequest& request, StorageRequest const& req) {
  if (req.template HasOption<PredefinedAcl>()) {
    request.set_predefined_acl(
        GrpcClient::ToProtoObject(req.template GetOption<PredefinedAcl>()));
  }
}

template <typename GrpcRequest, typename StorageRequest>
void SetPredefinedDefaultObjectAcl(GrpcRequest& request,
                                   StorageRequest const& req) {
  if (req.template HasOption<PredefinedDefaultObjectAcl>()) {
    request.set_predefined_default_object_acl(GrpcClient::ToProto(
        req.template GetOption<PredefinedDefaultObjectAcl>()));
  }
}

template <typename GrpcRequest, typename StorageRequest>
void SetMetagenerationConditions(GrpcRequest& request,
                                 StorageRequest const& req) {
  if (req.template HasOption<IfMetagenerationMatch>()) {
    request.set_if_metageneration_match(
        req.template GetOption<IfMetagenerationMatch>().value());
  }
  if (req.template HasOption<IfMetagenerationNotMatch>()) {
    request.set_if_metageneration_not_match(
        req.template GetOption<IfMetagenerationNotMatch>().value());
  }
}

template <typename GrpcRequest, typename StorageRequest>
void SetGenerationConditions(GrpcRequest& request, StorageRequest const& req) {
  if (req.template HasOption<IfGenerationMatch>()) {
    request.set_if_generation_match(
        req.template GetOption<IfGenerationMatch>().value());
  }
  if (req.template HasOption<IfGenerationNotMatch>()) {
    request.set_if_generation_not_match(
        req.template GetOption<IfGenerationNotMatch>().value());
  }
}

template <typename StorageRequest>
void SetResourceOptions(google::storage::v2::Object& resource,
                        StorageRequest const& request) {
  if (request.template HasOption<ContentEncoding>()) {
    resource.set_content_encoding(
        request.template GetOption<ContentEncoding>().value());
  }
  if (request.template HasOption<ContentType>()) {
    resource.set_content_type(
        request.template GetOption<ContentType>().value());
  }
  if (request.template HasOption<KmsKeyName>()) {
    resource.set_kms_key(request.template GetOption<KmsKeyName>().value());
  }
}

template <typename StorageRequest>
Status SetObjectMetadata(google::storage::v2::Object& resource,
                         StorageRequest const& req) {
  if (!req.template HasOption<WithObjectMetadata>()) {
    return Status{};
  }
  auto metadata = req.template GetOption<WithObjectMetadata>().value();
  if (!metadata.content_encoding().empty()) {
    resource.set_content_encoding(metadata.content_encoding());
  }
  if (!metadata.content_disposition().empty()) {
    resource.set_content_disposition(metadata.content_disposition());
  }
  if (!metadata.cache_control().empty()) {
    resource.set_cache_control(metadata.cache_control());
  }
  for (auto const& acl : metadata.acl()) {
    *resource.add_acl() = GrpcClient::ToProto(acl);
  }
  if (!metadata.content_language().empty()) {
    resource.set_content_language(metadata.content_language());
  }
  if (!metadata.content_type().empty()) {
    resource.set_content_type(metadata.content_type());
  }
  if (metadata.event_based_hold()) {
    resource.set_event_based_hold(metadata.event_based_hold());
  }

  for (auto const& kv : metadata.metadata()) {
    (*resource.mutable_metadata())[kv.first] = kv.second;
  }

  if (!metadata.storage_class().empty()) {
    resource.set_storage_class(metadata.storage_class());
  }
  resource.set_temporary_hold(metadata.temporary_hold());

  if (metadata.has_customer_encryption()) {
    auto encryption = GrpcClient::ToProto(metadata.customer_encryption());
    if (!encryption) return std::move(encryption).status();
    *resource.mutable_customer_encryption() = *std::move(encryption);
  }
  return Status{};
}

CustomerEncryption GrpcClient::FromProto(
    google::storage::v2::Object::CustomerEncryption rhs) {
  CustomerEncryption result;
  result.encryption_algorithm = std::move(*rhs.mutable_encryption_algorithm());
  result.key_sha256 = Base64Encode(rhs.key_sha256_bytes());
  return result;
}

StatusOr<google::storage::v2::Object::CustomerEncryption> GrpcClient::ToProto(
    CustomerEncryption rhs) {
  auto key_sha256 = Base64Decode(rhs.key_sha256);
  if (!key_sha256) return std::move(key_sha256).status();
  google::storage::v2::Object::CustomerEncryption result;
  result.set_encryption_algorithm(std::move(rhs.encryption_algorithm));
  result.set_key_sha256_bytes(
      std::string(key_sha256->begin(), key_sha256->end()));
  return result;
}

ObjectMetadata GrpcClient::FromProto(google::storage::v2::Object object,
                                     Options const& options) {
  auto bucket_id = [](google::storage::v2::Object const& object) {
    auto const& bucket_name = object.bucket();
    auto const pos = bucket_name.find_last_of('/');
    if (pos == std::string::npos) return bucket_name;
    return bucket_name.substr(pos + 1);
  };

  ObjectMetadata metadata;
  metadata.kind_ = "storage#object";
  metadata.bucket_ = bucket_id(object);
  metadata.name_ = std::move(*object.mutable_name());
  metadata.generation_ = object.generation();
  metadata.id_ = metadata.bucket() + "/" + metadata.name() + "/" +
                 std::to_string(metadata.generation());
  auto const metadata_endpoint = [&options]() -> std::string {
    if (options.get<RestEndpointOption>() != "https://storage.googleapis.com") {
      return options.get<RestEndpointOption>();
    }
    return "https://www.googleapis.com";
  }();
  auto const path = [&options]() -> std::string {
    if (!options.has<TargetApiVersionOption>()) return "/storage/v1";
    return "/storage/" + options.get<TargetApiVersionOption>();
  }();
  auto const rel_path = "/b/" + metadata.bucket() + "/o/" + metadata.name();
  metadata.self_link_ = metadata_endpoint + path + rel_path;
  metadata.media_link_ =
      options.get<RestEndpointOption>() + "/download" + path + rel_path +
      "?generation=" + std::to_string(metadata.generation()) + "&alt=media";

  metadata.metageneration_ = object.metageneration();
  if (object.has_owner()) {
    metadata.owner_ = FromProto(*object.mutable_owner());
  }
  metadata.storage_class_ = std::move(*object.mutable_storage_class());
  if (object.has_create_time()) {
    metadata.time_created_ =
        google::cloud::internal::ToChronoTimePoint(object.create_time());
  }
  if (object.has_update_time()) {
    metadata.updated_ =
        google::cloud::internal::ToChronoTimePoint(object.update_time());
  }
  std::vector<ObjectAccessControl> acl;
  acl.reserve(object.acl_size());
  for (auto& item : *object.mutable_acl()) {
    acl.push_back(FromProto(std::move(item), metadata.bucket(), metadata.name(),
                            metadata.generation()));
  }
  metadata.acl_ = std::move(acl);
  metadata.cache_control_ = std::move(*object.mutable_cache_control());
  metadata.component_count_ = object.component_count();
  metadata.content_disposition_ =
      std::move(*object.mutable_content_disposition());
  metadata.content_encoding_ = std::move(*object.mutable_content_encoding());
  metadata.content_language_ = std::move(*object.mutable_content_language());
  metadata.content_type_ = std::move(*object.mutable_content_type());
  if (object.has_checksums()) {
    if (object.checksums().has_crc32c()) {
      metadata.crc32c_ = Crc32cFromProto(object.checksums().crc32c());
    }
    if (!object.checksums().md5_hash().empty()) {
      metadata.md5_hash_ = MD5FromProto(object.checksums().md5_hash());
    }
  }
  if (object.has_customer_encryption()) {
    metadata.customer_encryption_ =
        FromProto(std::move(*object.mutable_customer_encryption()));
  }
  if (object.has_event_based_hold()) {
    metadata.event_based_hold_ = object.event_based_hold();
  }
  metadata.kms_key_name_ = std::move(*object.mutable_kms_key());

  for (auto const& kv : object.metadata()) {
    metadata.metadata_[kv.first] = kv.second;
  }
  if (object.has_retention_expire_time()) {
    metadata.retention_expiration_time_ =
        google::cloud::internal::ToChronoTimePoint(
            object.retention_expire_time());
  }
  metadata.size_ = static_cast<std::uint64_t>(object.size());
  metadata.temporary_hold_ = object.temporary_hold();
  if (object.has_delete_time()) {
    metadata.time_deleted_ =
        google::cloud::internal::ToChronoTimePoint(object.delete_time());
  }
  if (object.has_update_storage_class_time()) {
    metadata.time_storage_class_updated_ =
        google::cloud::internal::ToChronoTimePoint(
            object.update_storage_class_time());
  }
  if (object.has_custom_time()) {
    metadata.custom_time_ =
        google::cloud::internal::ToChronoTimePoint(object.custom_time());
  }

  return metadata;
}

google::storage::v2::ObjectAccessControl GrpcClient::ToProto(
    ObjectAccessControl const& acl) {
  google::storage::v2::ObjectAccessControl result;
  result.set_role(acl.role());
  result.set_id(acl.id());
  result.set_entity(acl.entity());
  result.set_entity_id(acl.entity_id());
  result.set_email(acl.email());
  result.set_domain(acl.domain());
  if (acl.has_project_team()) {
    result.mutable_project_team()->set_project_number(
        acl.project_team().project_number);
    result.mutable_project_team()->set_team(acl.project_team().team);
  }
  return result;
}

ObjectAccessControl GrpcClient::FromProto(
    google::storage::v2::ObjectAccessControl acl,
    std::string const& bucket_name, std::string const& object_name,
    std::uint64_t generation) {
  ObjectAccessControl result;
  result.kind_ = "storage#objectAccessControl";
  result.bucket_ = bucket_name;
  result.object_ = object_name;
  result.generation_ = generation;
  result.domain_ = std::move(*acl.mutable_domain());
  result.email_ = std::move(*acl.mutable_email());
  result.entity_ = std::move(*acl.mutable_entity());
  result.entity_id_ = std::move(*acl.mutable_entity_id());
  result.id_ = std::move(*acl.mutable_id());
  if (acl.has_project_team()) {
    result.project_team_ = ProjectTeam{
        std::move(*acl.mutable_project_team()->mutable_project_number()),
        std::move(*acl.mutable_project_team()->mutable_team()),
    };
  }
  result.role_ = std::move(*acl.mutable_role());
  result.self_link_.clear();

  return result;
}

google::storage::v2::Owner GrpcClient::ToProto(Owner rhs) {
  google::storage::v2::Owner result;
  *result.mutable_entity() = std::move(rhs.entity);
  *result.mutable_entity_id() = std::move(rhs.entity_id);
  return result;
}

Owner GrpcClient::FromProto(google::storage::v2::Owner rhs) {
  Owner result;
  result.entity = std::move(*rhs.mutable_entity());
  result.entity_id = std::move(*rhs.mutable_entity_id());
  return result;
}

google::storage::v2::PredefinedObjectAcl GrpcClient::ToProtoObject(
    PredefinedAcl const& acl) {
  if (acl.value() == PredefinedAcl::BucketOwnerFullControl().value()) {
    return google::storage::v2::OBJECT_ACL_BUCKET_OWNER_FULL_CONTROL;
  }
  if (acl.value() == PredefinedAcl::BucketOwnerRead().value()) {
    return google::storage::v2::OBJECT_ACL_BUCKET_OWNER_READ;
  }
  if (acl.value() == PredefinedAcl::AuthenticatedRead().value()) {
    return google::storage::v2::OBJECT_ACL_AUTHENTICATED_READ;
  }
  if (acl.value() == PredefinedAcl::Private().value()) {
    return google::storage::v2::OBJECT_ACL_PRIVATE;
  }
  if (acl.value() == PredefinedAcl::ProjectPrivate().value()) {
    return google::storage::v2::OBJECT_ACL_PROJECT_PRIVATE;
  }
  if (acl.value() == PredefinedAcl::PublicRead().value()) {
    return google::storage::v2::OBJECT_ACL_PUBLIC_READ;
  }
  if (acl.value() == PredefinedAcl::PublicReadWrite().value()) {
    GCP_LOG(ERROR) << "Invalid predefinedAcl value " << acl;
    return google::storage::v2::PREDEFINED_OBJECT_ACL_UNSPECIFIED;
  }
  GCP_LOG(ERROR) << "Unknown predefinedAcl value " << acl;
  return google::storage::v2::PREDEFINED_OBJECT_ACL_UNSPECIFIED;
}

StatusOr<google::storage::v2::WriteObjectRequest> GrpcClient::ToProto(
    InsertObjectMediaRequest const& request) {
  google::storage::v2::WriteObjectRequest r;
  auto& object_spec = *r.mutable_write_object_spec();
  auto& resource = *object_spec.mutable_resource();
  SetResourceOptions(resource, request);
  auto status = SetObjectMetadata(resource, request);
  if (!status.ok()) return status;
  SetPredefinedAcl(object_spec, request);
  SetGenerationConditions(object_spec, request);
  SetMetagenerationConditions(object_spec, request);
  status = SetCommonObjectParameters(r, request);
  if (!status.ok()) return status;
  SetCommonParameters(r, request);

  resource.set_bucket("projects/_/buckets/" + request.bucket_name());
  resource.set_name(request.object_name());
  r.set_write_offset(0);

  auto& checksums = *r.mutable_object_checksums();
  if (request.HasOption<Crc32cChecksumValue>()) {
    // The client library accepts CRC32C checksums in the format required by the
    // REST APIs (base64-encoded big-endian, 32-bit integers). We need to
    // convert this to the format expected by proto, which is just a 32-bit
    // integer. But the value received by the application might be incorrect, so
    // we need to validate it.
    auto as_proto =
        Crc32cToProto(request.GetOption<Crc32cChecksumValue>().value());
    if (!as_proto.ok()) return std::move(as_proto).status();
    checksums.set_crc32c(*as_proto);
  } else if (request.GetOption<DisableCrc32cChecksum>().value_or(false)) {
    // Nothing to do, the option is disabled (mostly useful in tests).
  } else {
    checksums.set_crc32c(crc32c::Crc32c(request.contents()));
  }

  if (request.HasOption<MD5HashValue>()) {
    auto as_proto = MD5ToProto(request.GetOption<MD5HashValue>().value());
    if (!as_proto.ok()) return std::move(as_proto).status();
    checksums.set_md5_hash(*std::move(as_proto));
  } else if (request.GetOption<DisableMD5Hash>().value_or(false)) {
    // Nothing to do, the option is disabled.
  } else {
    checksums.set_md5_hash(ComputeMD5Hash(request.contents()));
  }

  return r;
}

ResumableUploadResponse GrpcClient::FromProto(
    google::storage::v2::WriteObjectResponse const& p, Options const& options) {
  ResumableUploadResponse response;
  response.upload_state = ResumableUploadResponse::kInProgress;
  if (p.has_persisted_size() && p.persisted_size() > 0) {
    // TODO(#6880) - cleanup the committed_byte vs. size thing
    response.last_committed_byte =
        static_cast<std::uint64_t>(p.persisted_size()) - 1;
  } else {
    response.last_committed_byte = 0;
  }
  if (p.has_resource()) {
    response.payload = FromProto(p.resource(), options);
    response.upload_state = ResumableUploadResponse::kDone;
  }
  return response;
}

StatusOr<google::storage::v2::StartResumableWriteRequest> GrpcClient::ToProto(
    ResumableUploadRequest const& request) {
  google::storage::v2::StartResumableWriteRequest result;
  auto status = SetCommonObjectParameters(result, request);
  if (!status.ok()) return status;

  auto& object_spec = *result.mutable_write_object_spec();
  auto& resource = *object_spec.mutable_resource();
  SetResourceOptions(resource, request);
  status = SetObjectMetadata(resource, request);
  if (!status.ok()) return status;
  SetPredefinedAcl(object_spec, request);
  SetGenerationConditions(object_spec, request);
  SetMetagenerationConditions(object_spec, request);
  SetCommonParameters(result, request);

  resource.set_bucket("projects/_/buckets/" + request.bucket_name());
  resource.set_name(request.object_name());

  return result;
}

google::storage::v2::QueryWriteStatusRequest GrpcClient::ToProto(
    QueryResumableUploadRequest const& request) {
  google::storage::v2::QueryWriteStatusRequest r;
  r.set_upload_id(request.upload_session_url());
  return r;
}

StatusOr<google::storage::v2::ReadObjectRequest> GrpcClient::ToProto(
    ReadObjectRangeRequest const& request) {
  google::storage::v2::ReadObjectRequest r;
  auto status = SetCommonObjectParameters(r, request);
  if (!status.ok()) return status;
  r.set_object(request.object_name());
  r.set_bucket("projects/_/buckets/" + request.bucket_name());
  if (request.HasOption<Generation>()) {
    r.set_generation(request.GetOption<Generation>().value());
  }
  if (request.HasOption<ReadRange>()) {
    auto const range = request.GetOption<ReadRange>().value();
    r.set_read_offset(range.begin);
    r.set_read_limit(range.end - range.begin);
  }
  if (request.HasOption<ReadLast>()) {
    auto const offset = request.GetOption<ReadLast>().value();
    r.set_read_offset(-offset);
  }
  if (request.HasOption<ReadFromOffset>()) {
    auto const offset = request.GetOption<ReadFromOffset>().value();
    if (offset > r.read_offset()) {
      if (r.read_limit() > 0) {
        r.set_read_limit(offset - r.read_offset());
      }
      r.set_read_offset(offset);
    }
  }
  SetGenerationConditions(r, request);
  SetMetagenerationConditions(r, request);
  SetCommonParameters(r, request);

  return r;
}

std::string GrpcClient::Crc32cFromProto(std::uint32_t v) {
  auto endian_encoded = google::cloud::internal::EncodeBigEndian(v);
  return Base64Encode(endian_encoded);
}

StatusOr<std::uint32_t> GrpcClient::Crc32cToProto(std::string const& v) {
  auto decoded = Base64Decode(v);
  if (!decoded) return std::move(decoded).status();
  return google::cloud::internal::DecodeBigEndian<std::uint32_t>(
      std::string(decoded->begin(), decoded->end()));
}

std::string GrpcClient::MD5FromProto(std::string const& v) {
  return internal::Base64Encode(v);
}

StatusOr<std::string> GrpcClient::MD5ToProto(std::string const& v) {
  if (v.empty()) return {};
  auto binary = internal::Base64Decode(v);
  if (!binary) return std::move(binary).status();
  return std::string{binary->begin(), binary->end()};
}

std::string GrpcClient::ComputeMD5Hash(std::string const& payload) {
  auto b = internal::MD5Hash(payload);
  return std::string{b.begin(), b.end()};
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
