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
#include "google/cloud/storage/internal/grpc_object_read_source.h"
#include "google/cloud/storage/internal/grpc_resumable_upload_session.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/internal/resumable_upload_session.h"
#include "google/cloud/storage/internal/sha256_hash.h"
#include "google/cloud/storage/internal/storage_auth.h"
#include "google/cloud/storage/internal/storage_round_robin.h"
#include "google/cloud/storage/internal/storage_stub.h"
#include "google/cloud/storage/oauth2/anonymous_credentials.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/big_endian.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/streaming_read_rpc.h"
#include "google/cloud/internal/streaming_write_rpc.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/log.h"
#include "absl/algorithm/container.h"
#include "absl/strings/str_split.h"
#include "absl/time/time.h"
#include <crc32c/crc32c.h>
#include <grpcpp/grpcpp.h>
#include <algorithm>
#include <cinttypes>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

using ::google::cloud::internal::GrpcAuthenticationStrategy;

auto constexpr kDirectPathConfig = R"json({
    "loadBalancingConfig": [{
      "grpclb": {
        "childPolicy": [{
          "pick_first": {}
        }]
      }
    }]
  })json";

bool DirectPathEnabled() {
  auto const direct_path_settings =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_ENABLE_DIRECT_PATH")
          .value_or("");
  return absl::c_any_of(absl::StrSplit(direct_path_settings, ','),
                        [](absl::string_view v) { return v == "storage"; });
}

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

  if (!options.has<GrpcBackgroundThreadsFactoryOption>()) {
    options.set<GrpcBackgroundThreadsFactoryOption>(
        google::cloud::internal::DefaultBackgroundThreadsFactory);
  }
  if (!options.has<GrpcNumChannelsOption>()) {
    options.set<GrpcNumChannelsOption>(DefaultGrpcNumChannels());
  }
  return options;
}

std::shared_ptr<grpc::Channel> CreateGrpcChannel(
    GrpcAuthenticationStrategy& auth, Options const& options, int channel_id) {
  grpc::ChannelArguments args;
  args.SetInt("grpc.channel_id", channel_id);
  if (DirectPathEnabled()) {
    args.SetServiceConfigJSON(kDirectPathConfig);
  }
  return auth.CreateChannel(options.get<EndpointOption>(), std::move(args));
}

std::shared_ptr<GrpcAuthenticationStrategy> CreateAuthenticationStrategy(
    CompletionQueue cq, Options const& opts) {
  if (opts.has<UnifiedCredentialsOption>()) {
    return google::cloud::internal::CreateAuthenticationStrategy(
        opts.get<UnifiedCredentialsOption>(), std::move(cq), opts);
  }
  return google::cloud::internal::CreateAuthenticationStrategy(
      opts.get<google::cloud::GrpcCredentialOption>());
}

std::shared_ptr<StorageStub> CreateStorageStub(CompletionQueue cq,
                                               Options const& opts) {
  auto auth = CreateAuthenticationStrategy(std::move(cq), opts);
  std::vector<std::shared_ptr<StorageStub>> children(
      opts.get<GrpcNumChannelsOption>());
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

std::shared_ptr<GrpcClient> GrpcClient::Create(Options const& opts) {
  // Cannot use std::make_shared<> as the constructor is private.
  return std::shared_ptr<GrpcClient>(new GrpcClient(opts));
}

GrpcClient::GrpcClient(Options const& opts)
    : backwards_compatibility_options_(
          MakeBackwardsCompatibleClientOptions(opts)),
      background_(opts.get<GrpcBackgroundThreadsFactoryOption>()()),
      stub_(CreateStorageStub(background_->cq(), opts)) {}

std::unique_ptr<GrpcClient::InsertStream> GrpcClient::CreateUploadWriter(
    std::unique_ptr<grpc::ClientContext> context) {
  return stub_->InsertObjectMedia(std::move(context));
}

StatusOr<ResumableUploadResponse> GrpcClient::QueryResumableUpload(
    QueryResumableUploadRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->QueryWriteStatus(context, ToProto(request));
  if (!response) return std::move(response).status();

  return ResumableUploadResponse{
      {},
      static_cast<std::uint64_t>(response->committed_size()),
      // TODO(b/146890058) - `response` should include the object metadata.
      ObjectMetadata{},
      response->complete() ? ResumableUploadResponse::kDone
                           : ResumableUploadResponse::kInProgress,
      {}};
}

ClientOptions const& GrpcClient::client_options() const {
  return backwards_compatibility_options_;
}

StatusOr<ListBucketsResponse> GrpcClient::ListBuckets(
    ListBucketsRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->ListBuckets(context, ToProto(request));
  if (!response) return std::move(response).status();

  ListBucketsResponse res;
  res.next_page_token = std::move(*response->mutable_next_page_token());
  for (auto& item : *response->mutable_items()) {
    res.items.emplace_back(FromProto(std::move(item)));
  }

  return res;
}

StatusOr<BucketMetadata> GrpcClient::CreateBucket(
    CreateBucketRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->InsertBucket(context, ToProto(request));
  if (!response) return std::move(response).status();
  return FromProto(*std::move(response));
}

StatusOr<BucketMetadata> GrpcClient::GetBucketMetadata(
    GetBucketMetadataRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->GetBucket(context, ToProto(request));
  if (!response) return std::move(response).status();
  return FromProto(*std::move(response));
}

StatusOr<EmptyResponse> GrpcClient::DeleteBucket(
    DeleteBucketRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->DeleteBucket(context, ToProto(request));
  if (!response.ok()) return response;
  return EmptyResponse{};
}

StatusOr<BucketMetadata> GrpcClient::UpdateBucket(
    UpdateBucketRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->UpdateBucket(context, ToProto(request));
  if (!response.ok()) return std::move(response).status();
  return FromProto(*std::move(response));
}

StatusOr<BucketMetadata> GrpcClient::PatchBucket(PatchBucketRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<IamPolicy> GrpcClient::GetBucketIamPolicy(
    GetBucketIamPolicyRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<NativeIamPolicy> GrpcClient::GetNativeBucketIamPolicy(
    GetBucketIamPolicyRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->GetBucketIamPolicy(context, ToProto(request));
  if (!response.ok()) return std::move(response).status();
  return FromProto(*std::move(response));
}

StatusOr<IamPolicy> GrpcClient::SetBucketIamPolicy(
    SetBucketIamPolicyRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<NativeIamPolicy> GrpcClient::SetNativeBucketIamPolicy(
    SetNativeBucketIamPolicyRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->SetBucketIamPolicy(context, ToProto(request));
  if (!response.ok()) return std::move(response).status();
  return FromProto(*std::move(response));
}

StatusOr<TestBucketIamPermissionsResponse> GrpcClient::TestBucketIamPermissions(
    TestBucketIamPermissionsRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->TestBucketIamPermissions(context, ToProto(request));
  if (!response.ok()) return std::move(response).status();

  TestBucketIamPermissionsResponse res;
  for (auto const& permission : *response->mutable_permissions()) {
    res.permissions.emplace_back(std::move(permission));
  }

  return res;
}

StatusOr<BucketMetadata> GrpcClient::LockBucketRetentionPolicy(
    LockBucketRetentionPolicyRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<ObjectMetadata> GrpcClient::InsertObjectMedia(
    InsertObjectMediaRequest const& request) {
  auto stream =
      stub_->InsertObjectMedia(absl::make_unique<grpc::ClientContext>());

  auto proto_request = ToProto(request);
  auto const& contents = request.contents();
  auto const contents_size = static_cast<std::int64_t>(contents.size());
  std::int64_t const maximum_buffer_size =
      google::storage::v1::ServiceConstants::MAX_WRITE_CHUNK_BYTES;

  // This loop must run at least once because we need to send at least one
  // Write() call for empty objects.
  for (std::int64_t offset = 0, n = 0; offset <= contents_size; offset += n) {
    proto_request.set_write_offset(offset);
    auto& data = *proto_request.mutable_checksummed_data();
    n = (std::min)(contents_size - offset, maximum_buffer_size);
    data.set_content(
        contents.substr(static_cast<std::string::size_type>(offset),
                        static_cast<std::string::size_type>(n)));
    data.mutable_crc32c()->set_value(crc32c::Crc32c(data.content()));

    if (offset + n >= contents_size) {
      proto_request.set_finish_write(true);
      stream->Write(proto_request, grpc::WriteOptions{}.set_last_message());
      break;
    }
    if (!stream->Write(proto_request, grpc::WriteOptions{})) break;
    // After the first message, clear the object specification and checksums,
    // there is no need to resend it.
    proto_request.clear_insert_object_spec();
    proto_request.clear_object_checksums();
  }

  auto response = stream->Close();
  if (!response) return std::move(response).status();
  return FromProto(*std::move(response));
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
  return std::unique_ptr<ObjectReadSource>(
      absl::make_unique<GrpcObjectReadSource>(stub_->GetObjectMedia(
          absl::make_unique<grpc::ClientContext>(), ToProto(request))));
}

StatusOr<ListObjectsResponse> GrpcClient::ListObjects(
    ListObjectsRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<EmptyResponse> GrpcClient::DeleteObject(
    DeleteObjectRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->DeleteObject(context, ToProto(request));
  if (!response.ok()) return response;
  return EmptyResponse{};
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
  if (request.HasOption<UseResumableUploadSession>()) {
    auto session_id = request.GetOption<UseResumableUploadSession>().value();
    if (!session_id.empty()) {
      return RestoreResumableSession(session_id);
    }
  }

  grpc::ClientContext context;
  auto response = stub_->StartResumableWrite(context, ToProto(request));
  if (!response.ok()) return std::move(response).status();

  auto self = shared_from_this();
  return std::unique_ptr<ResumableUploadSession>(new GrpcResumableUploadSession(
      self,
      {request.bucket_name(), request.object_name(), response->upload_id()}));
}

StatusOr<std::unique_ptr<ResumableUploadSession>>
GrpcClient::RestoreResumableSession(std::string const& upload_url) {
  auto self = shared_from_this();
  auto upload_session_params = DecodeGrpcResumableUploadSessionUrl(upload_url);
  if (!upload_session_params) {
    return upload_session_params.status();
  }
  auto session = std::unique_ptr<ResumableUploadSession>(
      new GrpcResumableUploadSession(self, *upload_session_params));
  auto response = session->ResetSession();
  if (response.status().ok()) {
    return session;
  }
  return std::move(response).status();
}

StatusOr<EmptyResponse> GrpcClient::DeleteResumableUpload(
    DeleteResumableUploadRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->DeleteObject(context, ToProto(request));
  if (!response.ok()) return response;
  return EmptyResponse{};
}

StatusOr<ListBucketAclResponse> GrpcClient::ListBucketAcl(
    ListBucketAclRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->ListBucketAccessControls(context, ToProto(request));
  if (!response.ok()) return std::move(response).status();

  ListBucketAclResponse res;
  for (auto& item : *response->mutable_items()) {
    res.items.emplace_back(FromProto(std::move(item)));
  }

  return res;
}

StatusOr<BucketAccessControl> GrpcClient::GetBucketAcl(
    GetBucketAclRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->GetBucketAccessControl(context, ToProto(request));
  if (!response.ok()) return std::move(response).status();
  return FromProto(*std::move(response));
}

StatusOr<BucketAccessControl> GrpcClient::CreateBucketAcl(
    CreateBucketAclRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->InsertBucketAccessControl(context, ToProto(request));
  if (!response.ok()) return std::move(response).status();
  return FromProto(*std::move(response));
}

StatusOr<EmptyResponse> GrpcClient::DeleteBucketAcl(
    DeleteBucketAclRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->DeleteBucketAccessControl(context, ToProto(request));
  if (!response.ok()) return response;
  return EmptyResponse{};
}

StatusOr<ListObjectAclResponse> GrpcClient::ListObjectAcl(
    ListObjectAclRequest const&) {
  return Status(StatusCode::kUnimplemented, __func__);
}

StatusOr<BucketAccessControl> GrpcClient::UpdateBucketAcl(
    UpdateBucketAclRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->UpdateBucketAccessControl(context, ToProto(request));
  if (!response.ok()) return std::move(response).status();
  return FromProto(*std::move(response));
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
    ListDefaultObjectAclRequest const& request) {
  grpc::ClientContext context;
  auto response =
      stub_->ListDefaultObjectAccessControls(context, ToProto(request));
  if (!response.ok()) return std::move(response).status();

  ListDefaultObjectAclResponse res;
  for (auto& item : *response->mutable_items()) {
    res.items.emplace_back(FromProto(std::move(item)));
  }

  return res;
}

StatusOr<ObjectAccessControl> GrpcClient::CreateDefaultObjectAcl(
    CreateDefaultObjectAclRequest const& request) {
  grpc::ClientContext context;
  auto response =
      stub_->InsertDefaultObjectAccessControl(context, ToProto(request));
  if (!response.ok()) return std::move(response).status();
  return FromProto(*std::move(response));
}

StatusOr<EmptyResponse> GrpcClient::DeleteDefaultObjectAcl(
    DeleteDefaultObjectAclRequest const& request) {
  grpc::ClientContext context;
  auto response =
      stub_->DeleteDefaultObjectAccessControl(context, ToProto(request));
  if (!response.ok()) return response;
  return EmptyResponse{};
}

StatusOr<ObjectAccessControl> GrpcClient::GetDefaultObjectAcl(
    GetDefaultObjectAclRequest const& request) {
  grpc::ClientContext context;
  auto response =
      stub_->GetDefaultObjectAccessControl(context, ToProto(request));
  if (!response.ok()) return std::move(response).status();
  return FromProto(*std::move(response));
}

StatusOr<ObjectAccessControl> GrpcClient::UpdateDefaultObjectAcl(
    UpdateDefaultObjectAclRequest const& request) {
  grpc::ClientContext context;
  auto response =
      stub_->UpdateDefaultObjectAccessControl(context, ToProto(request));
  if (!response.ok()) return std::move(response).status();
  return FromProto(*std::move(response));
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

google::storage::v1::Notification GrpcClient::ToProto(
    NotificationMetadata const& notification) {
  google::storage::v1::Notification result;
  result.set_topic(notification.topic());
  result.set_etag(notification.etag());
  result.set_object_name_prefix(notification.object_name_prefix());
  result.set_payload_format(notification.payload_format());
  result.set_id(notification.id());

  for (auto const& v : notification.event_types()) {
    result.add_event_types(v);
  }
  for (auto const& kv : notification.custom_attributes()) {
    (*result.mutable_custom_attributes())[kv.first] = kv.second;
  }

  return result;
}

NotificationMetadata GrpcClient::FromProto(
    google::storage::v1::Notification notification) {
  NotificationMetadata result{notification.id(), notification.etag()};
  result.set_topic(notification.topic())
      .set_object_name_prefix(notification.object_name_prefix())
      .set_payload_format(notification.payload_format());

  for (auto& v : *notification.mutable_event_types()) {
    result.append_event_type(std::move(v));
  }
  for (auto& kv : *notification.mutable_custom_attributes()) {
    result.upsert_custom_attributes(std::move(kv.first), std::move(kv.second));
  }

  return result;
}

StatusOr<ListNotificationsResponse> GrpcClient::ListNotifications(
    ListNotificationsRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->ListNotifications(context, ToProto(request));
  if (!response.ok()) return std::move(response).status();

  ListNotificationsResponse res;
  for (auto& item : *response->mutable_items()) {
    res.items.emplace_back(FromProto(std::move(item)));
  }

  return res;
}

StatusOr<NotificationMetadata> GrpcClient::CreateNotification(
    CreateNotificationRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->InsertNotification(context, ToProto(request));
  if (!response.ok()) return std::move(response).status();
  return FromProto(*std::move(response));
}

StatusOr<NotificationMetadata> GrpcClient::GetNotification(
    GetNotificationRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->GetNotification(context, ToProto(request));
  if (!response.ok()) return std::move(response).status();
  return FromProto(*std::move(response));
}

StatusOr<EmptyResponse> GrpcClient::DeleteNotification(
    DeleteNotificationRequest const& request) {
  grpc::ClientContext context;
  auto response = stub_->DeleteNotification(context, ToProto(request));
  if (!response.ok()) return response;
  return EmptyResponse{};
}

template <typename GrpcRequest, typename StorageRequest>
void SetCommonParameters(GrpcRequest& request, StorageRequest const& req) {
  if (req.template HasOption<UserProject>()) {
    request.mutable_common_request_params()->set_user_project(
        req.template GetOption<UserProject>().value());
  }
  // The gRPC has a single field for the `QuotaUser` parameter, while the JSON
  // API has two:
  //    https://cloud.google.com/storage/docs/json_api/v1/parameters#quotaUser
  // Fortunately the semantics are to use `quotaUser` if set, so we can set
  // the `UserIp` value into the `quota_user` field, and overwrite it if
  // `QuotaUser` is also set. A bit bizarre, but at least it is backwards
  // compatible.
  if (req.template HasOption<UserIp>()) {
    request.mutable_common_request_params()->set_quota_user(
        req.template GetOption<UserIp>().value());
  }
  if (req.template HasOption<QuotaUser>()) {
    request.mutable_common_request_params()->set_quota_user(
        req.template GetOption<QuotaUser>().value());
  }
  // TODO(#4215) - what do we do with FieldMask, as the representation for
  // `fields` is different.
}

template <typename GrpcRequest, typename StorageRequest>
void SetCommonObjectParameters(GrpcRequest& request,
                               StorageRequest const& req) {
  if (req.template HasOption<EncryptionKey>()) {
    auto data = req.template GetOption<EncryptionKey>().value();
    request.mutable_common_object_request_params()->set_encryption_algorithm(
        std::move(data.algorithm));
    request.mutable_common_object_request_params()->set_encryption_key(
        std::move(data.key));
    request.mutable_common_object_request_params()->set_encryption_key_sha256(
        std::move(data.sha256));
  }
}

template <typename GrpcRequest, typename StorageRequest>
void SetProjection(GrpcRequest& request, StorageRequest const& req) {
  if (req.template HasOption<Projection>()) {
    request.set_projection(
        GrpcClient::ToProto(req.template GetOption<Projection>()));
  }
}

template <typename GrpcRequest>
struct GetPredefinedAcl {
  auto operator()(GrpcRequest const& q) -> decltype(q.predefined_acl());
};

template <
    typename GrpcRequest, typename StorageRequest,
    typename std::enable_if<
        std::is_same<google::storage::v1::CommonEnums::PredefinedBucketAcl,
                     google::cloud::internal::invoke_result_t<
                         GetPredefinedAcl<GrpcRequest>, GrpcRequest>>::value,
        int>::type = 0>
void SetPredefinedAcl(GrpcRequest& request, StorageRequest const& req) {
  if (req.template HasOption<PredefinedAcl>()) {
    request.set_predefined_acl(
        GrpcClient::ToProtoBucket(req.template GetOption<PredefinedAcl>()));
  }
}

template <
    typename GrpcRequest, typename StorageRequest,
    typename std::enable_if<
        std::is_same<google::storage::v1::CommonEnums::PredefinedObjectAcl,
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
    request.mutable_if_metageneration_match()->set_value(
        req.template GetOption<IfMetagenerationMatch>().value());
  }
  if (req.template HasOption<IfMetagenerationNotMatch>()) {
    request.mutable_if_metageneration_not_match()->set_value(
        req.template GetOption<IfMetagenerationNotMatch>().value());
  }
}

template <typename GrpcRequest, typename StorageRequest>
void SetGenerationConditions(GrpcRequest& request, StorageRequest const& req) {
  if (req.template HasOption<IfGenerationMatch>()) {
    request.mutable_if_generation_match()->set_value(
        req.template GetOption<IfGenerationMatch>().value());
  }
  if (req.template HasOption<IfGenerationNotMatch>()) {
    request.mutable_if_generation_not_match()->set_value(
        req.template GetOption<IfGenerationNotMatch>().value());
  }
}

template <typename StorageRequest>
void SetResourceOptions(google::storage::v1::Object& resource,
                        StorageRequest const& request) {
  if (request.template HasOption<ContentEncoding>()) {
    resource.set_content_encoding(
        request.template GetOption<ContentEncoding>().value());
  }
  if (request.template HasOption<ContentType>()) {
    resource.set_content_type(
        request.template GetOption<ContentType>().value());
  }

  if (request.template HasOption<Crc32cChecksumValue>()) {
    resource.mutable_crc32c()->set_value(GrpcClient::Crc32cToProto(
        request.template GetOption<Crc32cChecksumValue>().value()));
  }
  if (request.template HasOption<MD5HashValue>()) {
    resource.set_md5_hash(request.template GetOption<MD5HashValue>().value());
  }
  if (request.template HasOption<KmsKeyName>()) {
    resource.set_kms_key_name(request.template GetOption<KmsKeyName>().value());
  }
}

template <typename StorageRequest>
void SetObjectMetadata(google::storage::v1::Object& resource,
                       StorageRequest const& req) {
  if (!req.template HasOption<WithObjectMetadata>()) {
    return;
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
    resource.mutable_event_based_hold()->set_value(metadata.event_based_hold());
  }

  for (auto const& kv : metadata.metadata()) {
    (*resource.mutable_metadata())[kv.first] = kv.second;
  }

  if (!metadata.storage_class().empty()) {
    resource.set_storage_class(metadata.storage_class());
  }
  resource.set_temporary_hold(metadata.temporary_hold());
}

BucketMetadata GrpcClient::FromProto(google::storage::v1::Bucket bucket) {
  BucketMetadata metadata;
  metadata.kind_ = "storage#bucket";
  // These are sorted as the fields in the proto, to make them easier to find
  // in the future.
  for (auto& v : *bucket.mutable_acl()) {
    metadata.acl_.push_back(FromProto(std::move(v)));
  }
  for (auto& v : *bucket.mutable_default_object_acl()) {
    metadata.default_acl_.push_back(FromProto(std::move(v)));
  }
  if (bucket.has_lifecycle()) {
    metadata.lifecycle_ = FromProto(std::move(*bucket.mutable_lifecycle()));
  }
  if (bucket.has_time_created()) {
    metadata.time_created_ =
        google::cloud::internal::ToChronoTimePoint(bucket.time_created());
  }
  metadata.id_ = std::move(*bucket.mutable_id());
  metadata.name_ = std::move(*bucket.mutable_name());
  metadata.project_number_ = bucket.project_number();
  metadata.metageneration_ = bucket.metageneration();
  for (auto& v : *bucket.mutable_cors()) {
    metadata.cors_.push_back(FromProto(std::move(v)));
  }
  metadata.location_ = std::move(*bucket.mutable_location());
  metadata.storage_class_ = std::move(*bucket.mutable_storage_class());
  metadata.etag_ = std::move(*bucket.mutable_etag());
  if (bucket.has_updated()) {
    metadata.updated_ =
        google::cloud::internal::ToChronoTimePoint(bucket.updated());
  }
  metadata.default_event_based_hold_ = bucket.default_event_based_hold();
  for (auto& kv : *bucket.mutable_labels()) {
    metadata.labels_.emplace(std::make_pair(kv.first, std::move(kv.second)));
  }
  if (bucket.has_website()) {
    metadata.website_ = FromProto(std::move(*bucket.mutable_website()));
  }
  if (bucket.has_versioning()) {
    metadata.versioning_ = FromProto(std::move(*bucket.mutable_versioning()));
  }
  if (bucket.has_logging()) {
    metadata.logging_ = FromProto(std::move(*bucket.mutable_logging()));
  }
  if (bucket.has_owner()) {
    metadata.owner_ = FromProto(std::move(*bucket.mutable_owner()));
  }
  if (bucket.has_encryption()) {
    metadata.encryption_ = FromProto(std::move(*bucket.mutable_encryption()));
  }
  if (bucket.has_billing()) {
    metadata.billing_ = FromProto(std::move(*bucket.mutable_billing()));
  }
  if (bucket.has_retention_policy()) {
    metadata.retention_policy_ =
        FromProto(std::move(*bucket.mutable_retention_policy()));
  }
  metadata.location_type_ = std::move(*bucket.mutable_location_type());
  if (bucket.has_iam_configuration()) {
    metadata.iam_configuration_ =
        FromProto(std::move(*bucket.mutable_iam_configuration()));
  }
  return metadata;
}

CustomerEncryption GrpcClient::FromProto(
    google::storage::v1::Object::CustomerEncryption rhs) {
  CustomerEncryption result;
  result.encryption_algorithm = std::move(*rhs.mutable_encryption_algorithm());
  result.key_sha256 = std::move(*rhs.mutable_key_sha256());
  return result;
}

google::storage::v1::Object::CustomerEncryption GrpcClient::ToProto(
    CustomerEncryption rhs) {
  google::storage::v1::Object::CustomerEncryption result;
  result.set_encryption_algorithm(std::move(rhs.encryption_algorithm));
  result.set_key_sha256(std::move(rhs.key_sha256));
  return result;
}

ObjectMetadata GrpcClient::FromProto(google::storage::v1::Object object) {
  ObjectMetadata metadata;
  metadata.etag_ = std::move(*object.mutable_etag());
  metadata.id_ = std::move(*object.mutable_id());
  metadata.kind_ = "storage#object";
  metadata.metageneration_ = object.metageneration();
  metadata.name_ = std::move(*object.mutable_name());
  if (object.has_owner()) {
    metadata.owner_ = FromProto(*object.mutable_owner());
  }
  metadata.storage_class_ = std::move(*object.mutable_storage_class());
  if (object.has_time_created()) {
    metadata.time_created_ =
        google::cloud::internal::ToChronoTimePoint(object.time_created());
  }
  if (object.has_updated()) {
    metadata.updated_ =
        google::cloud::internal::ToChronoTimePoint(object.updated());
  }
  std::vector<ObjectAccessControl> acl;
  acl.reserve(object.acl_size());
  for (auto& item : *object.mutable_acl()) {
    acl.push_back(FromProto(std::move(item)));
  }
  metadata.acl_ = std::move(acl);
  metadata.bucket_ = std::move(*object.mutable_bucket());
  metadata.cache_control_ = std::move(*object.mutable_cache_control());
  metadata.component_count_ = object.component_count();
  metadata.content_disposition_ =
      std::move(*object.mutable_content_disposition());
  metadata.content_encoding_ = std::move(*object.mutable_content_encoding());
  metadata.content_language_ = std::move(*object.mutable_content_language());
  metadata.content_type_ = std::move(*object.mutable_content_type());
  if (object.has_crc32c()) {
    metadata.crc32c_ = Crc32cFromProto(object.crc32c());
  }
  if (object.has_customer_encryption()) {
    metadata.customer_encryption_ =
        FromProto(std::move(*object.mutable_customer_encryption()));
  }
  if (object.has_event_based_hold()) {
    metadata.event_based_hold_ = object.event_based_hold().value();
  }
  metadata.generation_ = object.generation();
  metadata.kms_key_name_ = std::move(*object.mutable_kms_key_name());
  metadata.md5_hash_ = object.md5_hash();
  for (auto const& kv : object.metadata()) {
    metadata.metadata_[kv.first] = kv.second;
  }
  if (object.has_retention_expiration_time()) {
    metadata.retention_expiration_time_ =
        google::cloud::internal::ToChronoTimePoint(
            object.retention_expiration_time());
  }
  metadata.size_ = static_cast<std::uint64_t>(object.size());
  metadata.temporary_hold_ = object.temporary_hold();
  if (object.has_time_deleted()) {
    metadata.time_deleted_ =
        google::cloud::internal::ToChronoTimePoint(object.time_deleted());
  }
  if (object.has_time_storage_class_updated()) {
    metadata.time_storage_class_updated_ =
        google::cloud::internal::ToChronoTimePoint(
            object.time_storage_class_updated());
  }
  // TODO(#4893) - support customTime for GCS+gRPC

  return metadata;
}

google::storage::v1::ObjectAccessControl GrpcClient::ToProto(
    ObjectAccessControl const& acl) {
  google::storage::v1::ObjectAccessControl result;
  result.set_role(acl.role());
  result.set_etag(acl.etag());
  result.set_id(acl.id());
  result.set_bucket(acl.bucket());
  result.set_object(acl.object());
  result.set_generation(acl.generation());
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
    google::storage::v1::ObjectAccessControl acl) {
  ObjectAccessControl result;
  result.bucket_ = std::move(*acl.mutable_bucket());
  result.domain_ = std::move(*acl.mutable_domain());
  result.email_ = std::move(*acl.mutable_email());
  result.entity_ = std::move(*acl.mutable_entity());
  result.entity_id_ = std::move(*acl.mutable_entity_id());
  result.etag_ = std::move(*acl.mutable_etag());
  result.id_ = std::move(*acl.mutable_id());
  result.kind_ = "storage#objectAccessControl";
  if (acl.has_project_team()) {
    result.project_team_ = ProjectTeam{
        std::move(*acl.mutable_project_team()->mutable_project_number()),
        std::move(*acl.mutable_project_team()->mutable_team()),
    };
  }
  result.role_ = std::move(*acl.mutable_role());
  result.self_link_.clear();
  result.object_ = std::move(*acl.mutable_object());
  result.generation_ = acl.generation();

  return result;
}

google::storage::v1::BucketAccessControl GrpcClient::ToProto(
    BucketAccessControl const& acl) {
  google::storage::v1::BucketAccessControl result;
  result.set_role(acl.role());
  result.set_etag(acl.etag());
  result.set_id(acl.id());
  result.set_bucket(acl.bucket());
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

BucketAccessControl GrpcClient::FromProto(
    google::storage::v1::BucketAccessControl acl) {
  BucketAccessControl result;
  result.bucket_ = std::move(*acl.mutable_bucket());
  result.domain_ = std::move(*acl.mutable_domain());
  result.email_ = std::move(*acl.mutable_email());
  result.entity_ = std::move(*acl.mutable_entity());
  result.entity_id_ = std::move(*acl.mutable_entity_id());
  result.etag_ = std::move(*acl.mutable_etag());
  result.id_ = std::move(*acl.mutable_id());
  result.kind_ = "storage#bucketAccessControl";
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

google::storage::v1::Bucket::Billing GrpcClient::ToProto(
    BucketBilling const& rhs) {
  google::storage::v1::Bucket::Billing result;
  result.set_requester_pays(rhs.requester_pays);
  return result;
}

BucketBilling GrpcClient::FromProto(
    google::storage::v1::Bucket::Billing const& rhs) {
  BucketBilling result;
  result.requester_pays = rhs.requester_pays();
  return result;
}

google::storage::v1::Bucket::Cors GrpcClient::ToProto(CorsEntry const& rhs) {
  google::storage::v1::Bucket::Cors result;
  for (auto const& v : rhs.origin) {
    result.add_origin(v);
  }
  for (auto const& v : rhs.method) {
    result.add_method(v);
  }
  for (auto const& v : rhs.response_header) {
    result.add_response_header(v);
  }
  if (rhs.max_age_seconds.has_value()) {
    result.set_max_age_seconds(static_cast<std::int32_t>(*rhs.max_age_seconds));
  }
  return result;
}

CorsEntry GrpcClient::FromProto(google::storage::v1::Bucket::Cors const& rhs) {
  CorsEntry result;
  absl::c_copy(rhs.origin(), std::back_inserter(result.origin));
  absl::c_copy(rhs.method(), std::back_inserter(result.method));
  absl::c_copy(rhs.response_header(),
               std::back_inserter(result.response_header));
  result.max_age_seconds = rhs.max_age_seconds();
  return result;
}

google::storage::v1::Bucket::Encryption GrpcClient::ToProto(
    BucketEncryption const& rhs) {
  google::storage::v1::Bucket::Encryption result;
  result.set_default_kms_key_name(rhs.default_kms_key_name);
  return result;
}

BucketEncryption GrpcClient::FromProto(
    google::storage::v1::Bucket::Encryption const& rhs) {
  BucketEncryption result;
  result.default_kms_key_name = rhs.default_kms_key_name();
  return result;
}

google::storage::v1::Bucket::IamConfiguration GrpcClient::ToProto(
    BucketIamConfiguration const& rhs) {
  google::storage::v1::Bucket::IamConfiguration result;
  if (rhs.uniform_bucket_level_access.has_value()) {
    auto& ubla = *result.mutable_uniform_bucket_level_access();
    *ubla.mutable_locked_time() = google::cloud::internal::ToProtoTimestamp(
        rhs.uniform_bucket_level_access->locked_time);
    ubla.set_enabled(rhs.uniform_bucket_level_access->enabled);
  }
  return result;
}

BucketIamConfiguration GrpcClient::FromProto(
    google::storage::v1::Bucket::IamConfiguration const& rhs) {
  BucketIamConfiguration result;
  if (rhs.has_uniform_bucket_level_access()) {
    UniformBucketLevelAccess ubla;
    ubla.enabled = rhs.uniform_bucket_level_access().enabled();
    ubla.locked_time = google::cloud::internal::ToChronoTimePoint(
        rhs.uniform_bucket_level_access().locked_time());
    result.uniform_bucket_level_access = std::move(ubla);
  }
  return result;
}

google::storage::v1::Bucket::Logging GrpcClient::ToProto(
    BucketLogging const& rhs) {
  google::storage::v1::Bucket::Logging result;
  result.set_log_bucket(rhs.log_bucket);
  result.set_log_object_prefix(rhs.log_object_prefix);
  return result;
}

BucketLogging GrpcClient::FromProto(
    google::storage::v1::Bucket::Logging const& rhs) {
  BucketLogging result;
  result.log_bucket = rhs.log_bucket();
  result.log_object_prefix = rhs.log_object_prefix();
  return result;
}

google::storage::v1::Bucket::RetentionPolicy GrpcClient::ToProto(
    BucketRetentionPolicy const& rhs) {
  google::storage::v1::Bucket::RetentionPolicy result;
  *result.mutable_effective_time() =
      google::cloud::internal::ToProtoTimestamp(rhs.effective_time);
  result.set_is_locked(rhs.is_locked);
  result.set_retention_period(rhs.retention_period.count());
  return result;
}

BucketRetentionPolicy GrpcClient::FromProto(
    google::storage::v1::Bucket::RetentionPolicy const& rhs) {
  BucketRetentionPolicy result;
  result.effective_time =
      google::cloud::internal::ToChronoTimePoint(rhs.effective_time());
  result.is_locked = rhs.is_locked();
  result.retention_period = std::chrono::seconds(rhs.retention_period());
  return result;
}

google::storage::v1::Bucket::Versioning GrpcClient::ToProto(
    BucketVersioning const& rhs) {
  google::storage::v1::Bucket::Versioning result;
  result.set_enabled(rhs.enabled);
  return result;
}

BucketVersioning GrpcClient::FromProto(
    google::storage::v1::Bucket::Versioning const& rhs) {
  BucketVersioning result;
  result.enabled = rhs.enabled();
  return result;
}

google::storage::v1::Bucket::Website GrpcClient::ToProto(BucketWebsite rhs) {
  google::storage::v1::Bucket::Website result;
  result.set_main_page_suffix(std::move(rhs.main_page_suffix));
  result.set_not_found_page(std::move(rhs.not_found_page));
  return result;
}

BucketWebsite GrpcClient::FromProto(google::storage::v1::Bucket::Website rhs) {
  BucketWebsite result;
  result.main_page_suffix = std::move(*rhs.mutable_main_page_suffix());
  result.not_found_page = std::move(*rhs.mutable_not_found_page());
  return result;
}

google::storage::v1::Bucket::Lifecycle::Rule::Action GrpcClient::ToProto(
    LifecycleRuleAction rhs) {
  google::storage::v1::Bucket::Lifecycle::Rule::Action result;
  result.set_type(std::move(rhs.type));
  result.set_storage_class(std::move(rhs.storage_class));
  return result;
}

LifecycleRuleAction GrpcClient::FromProto(
    google::storage::v1::Bucket::Lifecycle::Rule::Action rhs) {
  LifecycleRuleAction result;
  result.type = std::move(*rhs.mutable_type());
  result.storage_class = std::move(*rhs.mutable_storage_class());
  return result;
}

google::storage::v1::Bucket::Lifecycle::Rule::Condition GrpcClient::ToProto(
    LifecycleRuleCondition rhs) {
  google::storage::v1::Bucket::Lifecycle::Rule::Condition result;
  if (rhs.age.has_value()) {
    result.set_age(*rhs.age);
  }
  if (rhs.created_before.has_value()) {
    auto const t = absl::FromCivil(*rhs.created_before, absl::UTCTimeZone());
    *result.mutable_created_before() =
        google::cloud::internal::ToProtoTimestamp(t);
  }
  if (rhs.is_live.has_value()) {
    result.mutable_is_live()->set_value(*rhs.is_live);
  }
  if (rhs.num_newer_versions.has_value()) {
    result.set_num_newer_versions(*rhs.num_newer_versions);
  }
  if (rhs.matches_storage_class.has_value()) {
    for (auto& v : *rhs.matches_storage_class) {
      *result.add_matches_storage_class() = std::move(v);
    }
  }
  return result;
}

LifecycleRuleCondition GrpcClient::FromProto(
    google::storage::v1::Bucket::Lifecycle::Rule::Condition rhs) {
  LifecycleRuleCondition result;
  if (rhs.age() != 0) {
    result.age = rhs.age();
  }
  if (rhs.has_created_before()) {
    auto const t = google::cloud::internal::ToAbslTime(rhs.created_before());
    result.created_before = absl::ToCivilDay(t, absl::UTCTimeZone());
  }
  if (rhs.has_is_live()) {
    result.is_live = rhs.is_live().value();
  }
  if (rhs.num_newer_versions() != 0) {
    result.num_newer_versions = rhs.num_newer_versions();
  }
  if (rhs.matches_storage_class_size() != 0) {
    std::vector<std::string> tmp;
    for (auto& v : *rhs.mutable_matches_storage_class()) {
      tmp.push_back(std::move(v));
    }
    result.matches_storage_class = std::move(tmp);
  }
  return result;
}

google::storage::v1::Bucket::Lifecycle::Rule GrpcClient::ToProto(
    LifecycleRule rhs) {
  google::storage::v1::Bucket::Lifecycle::Rule result;
  *result.mutable_action() = ToProto(std::move(rhs.action_));
  *result.mutable_condition() = ToProto(std::move(rhs.condition_));
  return result;
}

LifecycleRule GrpcClient::FromProto(
    google::storage::v1::Bucket::Lifecycle::Rule rhs) {
  LifecycleRuleAction action;
  LifecycleRuleCondition condition;
  if (rhs.has_action()) {
    action = FromProto(std::move(*rhs.mutable_action()));
  }
  if (rhs.has_condition()) {
    condition = FromProto(std::move(*rhs.mutable_condition()));
  }
  return LifecycleRule(std::move(condition), std::move(action));
}

google::storage::v1::Bucket::Lifecycle GrpcClient::ToProto(
    BucketLifecycle rhs) {
  google::storage::v1::Bucket::Lifecycle result;
  for (auto& v : rhs.rule) {
    *result.add_rule() = ToProto(std::move(v));
  }
  return result;
}

BucketLifecycle GrpcClient::FromProto(
    google::storage::v1::Bucket::Lifecycle rhs) {
  BucketLifecycle result;
  for (auto& v : *rhs.mutable_rule()) {
    result.rule.push_back(FromProto(std::move(v)));
  }
  return result;
}

google::storage::v1::Owner GrpcClient::ToProto(Owner rhs) {
  google::storage::v1::Owner result;
  *result.mutable_entity() = std::move(rhs.entity);
  *result.mutable_entity_id() = std::move(rhs.entity_id);
  return result;
}

Owner GrpcClient::FromProto(google::storage::v1::Owner rhs) {
  Owner result;
  result.entity = std::move(*rhs.mutable_entity());
  result.entity_id = std::move(*rhs.mutable_entity_id());
  return result;
}

NativeIamPolicy GrpcClient::FromProto(google::iam::v1::Policy rhs) {
  auto etag = std::move(rhs.etag());
  auto version = std::move(rhs.version());
  std::vector<NativeIamBinding> bindings;
  for (auto& binding : *rhs.mutable_bindings()) {
    bindings.emplace_back(
        std::move(binding.role()),
        std::vector<std::string>{binding.mutable_members()->begin(),
                                 binding.mutable_members()->end()});
  }
  return NativeIamPolicy{std::move(bindings), std::move(etag),
                         std::move(version)};
}

google::storage::v1::CommonEnums::Projection GrpcClient::ToProto(
    Projection const& p) {
  if (p.value() == Projection::NoAcl().value()) {
    return google::storage::v1::CommonEnums::NO_ACL;
  }
  if (p.value() == Projection::Full().value()) {
    return google::storage::v1::CommonEnums::FULL;
  }
  GCP_LOG(ERROR) << "Unknown projection value " << p;
  return google::storage::v1::CommonEnums::FULL;
}

google::storage::v1::CommonEnums::PredefinedBucketAcl GrpcClient::ToProtoBucket(
    PredefinedAcl const& acl) {
  if (acl.value() == PredefinedAcl::AuthenticatedRead().value()) {
    return google::storage::v1::CommonEnums::BUCKET_ACL_AUTHENTICATED_READ;
  }
  if (acl.value() == PredefinedAcl::Private().value()) {
    return google::storage::v1::CommonEnums::BUCKET_ACL_PRIVATE;
  }
  if (acl.value() == PredefinedAcl::ProjectPrivate().value()) {
    return google::storage::v1::CommonEnums::BUCKET_ACL_PROJECT_PRIVATE;
  }
  if (acl.value() == PredefinedAcl::PublicRead().value()) {
    return google::storage::v1::CommonEnums::BUCKET_ACL_PUBLIC_READ;
  }
  if (acl.value() == PredefinedAcl::PublicReadWrite().value()) {
    return google::storage::v1::CommonEnums::BUCKET_ACL_PUBLIC_READ_WRITE;
  }
  GCP_LOG(ERROR) << "Unknown predefinedAcl value " << acl;
  return google::storage::v1::CommonEnums::PREDEFINED_BUCKET_ACL_UNSPECIFIED;
}

google::storage::v1::CommonEnums::PredefinedObjectAcl GrpcClient::ToProtoObject(
    PredefinedAcl const& acl) {
  if (acl.value() == PredefinedAcl::BucketOwnerFullControl().value()) {
    return google::storage::v1::CommonEnums::
        OBJECT_ACL_BUCKET_OWNER_FULL_CONTROL;
  }
  if (acl.value() == PredefinedAcl::BucketOwnerRead().value()) {
    return google::storage::v1::CommonEnums::OBJECT_ACL_BUCKET_OWNER_READ;
  }
  if (acl.value() == PredefinedAcl::AuthenticatedRead().value()) {
    return google::storage::v1::CommonEnums::OBJECT_ACL_AUTHENTICATED_READ;
  }
  if (acl.value() == PredefinedAcl::Private().value()) {
    return google::storage::v1::CommonEnums::OBJECT_ACL_PRIVATE;
  }
  if (acl.value() == PredefinedAcl::ProjectPrivate().value()) {
    return google::storage::v1::CommonEnums::OBJECT_ACL_PROJECT_PRIVATE;
  }
  if (acl.value() == PredefinedAcl::PublicRead().value()) {
    return google::storage::v1::CommonEnums::OBJECT_ACL_PUBLIC_READ;
  }
  if (acl.value() == PredefinedAcl::PublicReadWrite().value()) {
    GCP_LOG(ERROR) << "Invalid predefinedAcl value " << acl;
    return google::storage::v1::CommonEnums::PREDEFINED_OBJECT_ACL_UNSPECIFIED;
  }
  GCP_LOG(ERROR) << "Unknown predefinedAcl value " << acl;
  return google::storage::v1::CommonEnums::PREDEFINED_OBJECT_ACL_UNSPECIFIED;
}

google::storage::v1::CommonEnums::PredefinedObjectAcl GrpcClient::ToProto(
    PredefinedDefaultObjectAcl const& acl) {
  if (acl.value() == PredefinedDefaultObjectAcl::AuthenticatedRead().value()) {
    return google::storage::v1::CommonEnums::OBJECT_ACL_AUTHENTICATED_READ;
  }
  if (acl.value() ==
      PredefinedDefaultObjectAcl::BucketOwnerFullControl().value()) {
    return google::storage::v1::CommonEnums::
        OBJECT_ACL_BUCKET_OWNER_FULL_CONTROL;
  }
  if (acl.value() == PredefinedDefaultObjectAcl::BucketOwnerRead().value()) {
    return google::storage::v1::CommonEnums::OBJECT_ACL_BUCKET_OWNER_READ;
  }
  if (acl.value() == PredefinedDefaultObjectAcl::Private().value()) {
    return google::storage::v1::CommonEnums::OBJECT_ACL_PRIVATE;
  }
  if (acl.value() == PredefinedDefaultObjectAcl::ProjectPrivate().value()) {
    return google::storage::v1::CommonEnums::OBJECT_ACL_PROJECT_PRIVATE;
  }
  if (acl.value() == PredefinedDefaultObjectAcl::PublicRead().value()) {
    return google::storage::v1::CommonEnums::OBJECT_ACL_PUBLIC_READ;
  }
  GCP_LOG(ERROR) << "Unknown predefinedAcl value " << acl;
  return google::storage::v1::CommonEnums::PREDEFINED_OBJECT_ACL_UNSPECIFIED;
}

google::storage::v1::Bucket GrpcClient::ToProto(
    BucketMetadata const& metadata) {
  google::storage::v1::Bucket bucket;
  // These are in the order of the proto fields, to make it easier to find them
  // later.
  for (auto const& v : metadata.acl()) {
    *bucket.add_acl() = ToProto(v);
  }
  for (auto const& v : metadata.default_acl()) {
    *bucket.add_default_object_acl() = ToProto(v);
  }
  if (metadata.has_lifecycle()) {
    *bucket.mutable_lifecycle() = ToProto(metadata.lifecycle());
  }
  *bucket.mutable_time_created() =
      google::cloud::internal::ToProtoTimestamp(metadata.time_created());
  bucket.set_id(metadata.id());
  bucket.set_name(metadata.name());
  bucket.set_project_number(metadata.project_number());
  bucket.set_metageneration(metadata.metageneration());
  for (auto const& v : metadata.cors()) {
    *bucket.add_cors() = ToProto(v);
  }
  bucket.set_location(metadata.location());
  bucket.set_storage_class(metadata.storage_class());
  bucket.set_etag(metadata.etag());
  *bucket.mutable_updated() =
      google::cloud::internal::ToProtoTimestamp(metadata.updated());
  bucket.set_default_event_based_hold(metadata.default_event_based_hold());
  for (auto const& kv : metadata.labels()) {
    (*bucket.mutable_labels())[kv.first] = kv.second;
  }
  if (metadata.has_website()) {
    *bucket.mutable_website() = ToProto(metadata.website());
  }
  if (metadata.has_versioning()) {
    *bucket.mutable_versioning() = ToProto(*metadata.versioning());
  }
  if (metadata.has_logging()) {
    *bucket.mutable_logging() = ToProto(metadata.logging());
  }
  if (metadata.has_owner()) {
    *bucket.mutable_owner() = ToProto(metadata.owner());
  }
  if (metadata.has_encryption()) {
    *bucket.mutable_encryption() = ToProto(metadata.encryption());
  }
  if (metadata.has_billing()) {
    *bucket.mutable_billing() = ToProto(metadata.billing());
  }
  if (metadata.has_retention_policy()) {
    *bucket.mutable_retention_policy() = ToProto(metadata.retention_policy());
  }
  bucket.set_location_type(metadata.location_type());
  if (metadata.has_iam_configuration()) {
    *bucket.mutable_iam_configuration() = ToProto(metadata.iam_configuration());
  }
  return bucket;
}

google::storage::v1::InsertBucketRequest GrpcClient::ToProto(
    CreateBucketRequest const& request) {
  google::storage::v1::InsertBucketRequest r;
  SetPredefinedAcl(r, request);
  SetPredefinedDefaultObjectAcl(r, request);
  r.set_project(request.project_id());
  SetProjection(r, request);
  *r.mutable_bucket() = ToProto(request.metadata());
  r.mutable_bucket()->set_name(request.metadata().name());
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::ListBucketsRequest GrpcClient::ToProto(
    ListBucketsRequest const& request) {
  google::storage::v1::ListBucketsRequest r;
  if (request.HasOption<MaxResults>()) {
    // The maximum page size is 1,000 anyway, if this cast
    // fails the request was invalid (but it can mask errors)
    r.set_max_results(
        static_cast<std::int32_t>(request.GetOption<MaxResults>().value()));
  }
  r.set_page_token(request.page_token());
  r.set_project(request.project_id());
  if (request.HasOption<Prefix>()) {
    r.set_prefix(request.GetOption<Prefix>().value());
  }
  SetProjection(r, request);
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::GetBucketRequest GrpcClient::ToProto(
    GetBucketMetadataRequest const& request) {
  google::storage::v1::GetBucketRequest r;
  r.set_bucket(request.bucket_name());
  SetMetagenerationConditions(r, request);
  SetProjection(r, request);
  SetCommonParameters(r, request);

  return r;
}

google::storage::v1::UpdateBucketRequest GrpcClient::ToProto(
    UpdateBucketRequest const& request) {
  google::storage::v1::UpdateBucketRequest r;
  r.set_bucket(request.metadata().name());
  *r.mutable_metadata() = ToProto(request.metadata());
  SetMetagenerationConditions(r, request);
  SetPredefinedAcl(r, request);
  SetPredefinedDefaultObjectAcl(r, request);
  SetProjection(r, request);
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::GetIamPolicyRequest GrpcClient::ToProto(
    GetBucketIamPolicyRequest const& request) {
  google::storage::v1::GetIamPolicyRequest r;
  auto& iam_request = *r.mutable_iam_request();
  iam_request.set_resource(request.bucket_name());
  if (request.HasOption<RequestedPolicyVersion>()) {
    int policy_version = static_cast<int>(
        request.template GetOption<RequestedPolicyVersion>().value());
    iam_request.mutable_options()->set_requested_policy_version(policy_version);
  }
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::SetIamPolicyRequest GrpcClient::ToProto(
    SetNativeBucketIamPolicyRequest const& request) {
  google::storage::v1::SetIamPolicyRequest r;
  google::iam::v1::Policy p;
  auto payload_policy = NativeIamPolicy::CreateFromJson(request.json_payload());
  r.mutable_iam_request()->set_resource(request.bucket_name());
  p.set_etag(payload_policy->etag());
  p.set_version(payload_policy->version());
  for (auto const& b : payload_policy->bindings()) {
    auto& new_binding = *p.add_bindings();
    new_binding.set_role(b.role());
    for (auto const& m : b.members()) {
      *new_binding.add_members() = std::move(m);
    }
  }
  *r.mutable_iam_request()->mutable_policy() = std::move(p);
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::TestIamPermissionsRequest GrpcClient::ToProto(
    TestBucketIamPermissionsRequest const& request) {
  google::storage::v1::TestIamPermissionsRequest r;
  r.mutable_iam_request()->set_resource(request.bucket_name());
  for (const auto& permission : request.permissions()) {
    *r.mutable_iam_request()->add_permissions() = std::move(permission);
  }
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::DeleteBucketRequest GrpcClient::ToProto(
    DeleteBucketRequest const& request) {
  google::storage::v1::DeleteBucketRequest r;
  r.set_bucket(request.bucket_name());
  SetMetagenerationConditions(r, request);
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::InsertBucketAccessControlRequest GrpcClient::ToProto(
    CreateBucketAclRequest const& request) {
  google::storage::v1::InsertBucketAccessControlRequest r;
  r.set_bucket(request.bucket_name());
  r.mutable_bucket_access_control()->set_role(request.role());
  r.mutable_bucket_access_control()->set_entity(request.entity());
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::ListBucketAccessControlsRequest GrpcClient::ToProto(
    ListBucketAclRequest const& request) {
  google::storage::v1::ListBucketAccessControlsRequest r;
  r.set_bucket(request.bucket_name());
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::GetBucketAccessControlRequest GrpcClient::ToProto(
    GetBucketAclRequest const& request) {
  google::storage::v1::GetBucketAccessControlRequest r;
  r.set_bucket(request.bucket_name());
  r.set_entity(request.entity());
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::UpdateBucketAccessControlRequest GrpcClient::ToProto(
    UpdateBucketAclRequest const& request) {
  google::storage::v1::UpdateBucketAccessControlRequest r;
  r.set_bucket(request.bucket_name());
  r.set_entity(request.entity());
  r.mutable_bucket_access_control()->set_role(request.role());
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::DeleteBucketAccessControlRequest GrpcClient::ToProto(
    DeleteBucketAclRequest const& request) {
  google::storage::v1::DeleteBucketAccessControlRequest r;
  r.set_bucket(request.bucket_name());
  r.set_entity(request.entity());
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::InsertDefaultObjectAccessControlRequest
GrpcClient::ToProto(CreateDefaultObjectAclRequest const& request) {
  google::storage::v1::InsertDefaultObjectAccessControlRequest r;
  r.set_bucket(request.bucket_name());
  r.mutable_object_access_control()->set_role(request.role());
  r.mutable_object_access_control()->set_entity(request.entity());
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::ListDefaultObjectAccessControlsRequest GrpcClient::ToProto(
    ListDefaultObjectAclRequest const& request) {
  google::storage::v1::ListDefaultObjectAccessControlsRequest r;
  r.set_bucket(request.bucket_name());
  SetMetagenerationConditions(r, request);
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::GetDefaultObjectAccessControlRequest GrpcClient::ToProto(
    GetDefaultObjectAclRequest const& request) {
  google::storage::v1::GetDefaultObjectAccessControlRequest r;
  r.set_bucket(request.bucket_name());
  r.set_entity(request.entity());
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::UpdateDefaultObjectAccessControlRequest
GrpcClient::ToProto(UpdateDefaultObjectAclRequest const& request) {
  google::storage::v1::UpdateDefaultObjectAccessControlRequest r;
  r.set_bucket(request.bucket_name());
  r.set_entity(request.entity());
  r.mutable_object_access_control()->set_role(request.role());
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::DeleteDefaultObjectAccessControlRequest
GrpcClient::ToProto(DeleteDefaultObjectAclRequest const& request) {
  google::storage::v1::DeleteDefaultObjectAccessControlRequest r;
  r.set_bucket(request.bucket_name());
  r.set_entity(request.entity());
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::InsertNotificationRequest GrpcClient::ToProto(
    CreateNotificationRequest const& request) {
  google::storage::v1::InsertNotificationRequest r;
  r.set_bucket(request.bucket_name());
  *r.mutable_notification() = ToProto(request.metadata());
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::ListNotificationsRequest GrpcClient::ToProto(
    ListNotificationsRequest const& request) {
  google::storage::v1::ListNotificationsRequest r;
  r.set_bucket(request.bucket_name());
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::GetNotificationRequest GrpcClient::ToProto(
    GetNotificationRequest const& request) {
  google::storage::v1::GetNotificationRequest r;
  r.set_bucket(request.bucket_name());
  r.set_notification(request.notification_id());
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::DeleteNotificationRequest GrpcClient::ToProto(
    DeleteNotificationRequest const& request) {
  google::storage::v1::DeleteNotificationRequest r;
  r.set_bucket(request.bucket_name());
  r.set_notification(request.notification_id());
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::InsertObjectRequest GrpcClient::ToProto(
    InsertObjectMediaRequest const& request) {
  google::storage::v1::InsertObjectRequest r;
  auto& object_spec = *r.mutable_insert_object_spec();
  auto& resource = *object_spec.mutable_resource();
  SetResourceOptions(resource, request);
  SetObjectMetadata(resource, request);
  SetPredefinedAcl(object_spec, request);
  SetGenerationConditions(object_spec, request);
  SetMetagenerationConditions(object_spec, request);
  SetProjection(object_spec, request);
  SetCommonObjectParameters(r, request);
  SetCommonParameters(r, request);

  resource.set_bucket(request.bucket_name());
  resource.set_name(request.object_name());
  r.set_write_offset(0);

  auto& checksums = *r.mutable_object_checksums();
  // TODO(#4156) - use the crc32c value in the request options.
  checksums.mutable_crc32c()->set_value(crc32c::Crc32c(request.contents()));
  // TODO(#4157) - use the MD5 hash value in the request options.
  checksums.set_md5_hash(MD5ToProto(ComputeMD5Hash(request.contents())));

  return r;
}

google::storage::v1::DeleteObjectRequest GrpcClient::ToProto(
    DeleteObjectRequest const& request) {
  google::storage::v1::DeleteObjectRequest r;
  r.set_bucket(request.bucket_name());
  r.set_object(request.object_name());
  if (request.HasOption<Generation>()) {
    r.set_generation(request.GetOption<Generation>().value());
  }
  SetGenerationConditions(r, request);
  SetMetagenerationConditions(r, request);
  SetCommonParameters(r, request);
  return r;
}

google::storage::v1::StartResumableWriteRequest GrpcClient::ToProto(
    ResumableUploadRequest const& request) {
  google::storage::v1::StartResumableWriteRequest result;

  auto& object_spec = *result.mutable_insert_object_spec();
  auto& resource = *object_spec.mutable_resource();
  SetResourceOptions(resource, request);
  SetObjectMetadata(resource, request);
  SetPredefinedAcl(object_spec, request);
  SetGenerationConditions(object_spec, request);
  SetMetagenerationConditions(object_spec, request);
  SetProjection(object_spec, request);
  SetCommonParameters(result, request);
  SetCommonObjectParameters(result, request);

  resource.set_bucket(request.bucket_name());
  resource.set_name(request.object_name());

  return result;
}

google::storage::v1::QueryWriteStatusRequest GrpcClient::ToProto(
    QueryResumableUploadRequest const& request) {
  google::storage::v1::QueryWriteStatusRequest r;
  r.set_upload_id(request.upload_session_url());
  return r;
}

google::storage::v1::DeleteObjectRequest GrpcClient::ToProto(
    DeleteResumableUploadRequest const& request) {
  auto upload_session_params =
      DecodeGrpcResumableUploadSessionUrl(request.upload_session_url());
  google::storage::v1::DeleteObjectRequest r;
  r.set_upload_id(upload_session_params->upload_id);
  return r;
}

google::storage::v1::GetObjectMediaRequest GrpcClient::ToProto(
    ReadObjectRangeRequest const& request) {
  google::storage::v1::GetObjectMediaRequest r;
  r.set_object(request.object_name());
  r.set_bucket(request.bucket_name());
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
  SetCommonObjectParameters(r, request);
  SetCommonParameters(r, request);

  return r;
}

std::string GrpcClient::Crc32cFromProto(
    google::protobuf::UInt32Value const& v) {
  auto endian_encoded = google::cloud::internal::EncodeBigEndian(v.value());
  return Base64Encode(endian_encoded);
}

std::uint32_t GrpcClient::Crc32cToProto(std::string const& v) {
  auto decoded = Base64Decode(v);
  return google::cloud::internal::DecodeBigEndian<std::uint32_t>(
             std::string(decoded.begin(), decoded.end()))
      .value();
}

std::string GrpcClient::MD5FromProto(std::string const& v) {
  if (v.empty()) return {};
  auto binary = internal::HexDecode(v);
  return internal::Base64Encode(binary);
}

std::string GrpcClient::MD5ToProto(std::string const& v) {
  if (v.empty()) return {};
  auto binary = internal::Base64Decode(v);
  return internal::HexEncode(binary);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
