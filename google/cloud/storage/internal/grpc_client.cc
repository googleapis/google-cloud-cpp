// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/storage/internal/crc32c.h"
#include "google/cloud/storage/internal/grpc_bucket_access_control_parser.h"
#include "google/cloud/storage/internal/grpc_bucket_metadata_parser.h"
#include "google/cloud/storage/internal/grpc_bucket_name.h"
#include "google/cloud/storage/internal/grpc_bucket_request_parser.h"
#include "google/cloud/storage/internal/grpc_configure_client_context.h"
#include "google/cloud/storage/internal/grpc_hmac_key_metadata_parser.h"
#include "google/cloud/storage/internal/grpc_hmac_key_request_parser.h"
#include "google/cloud/storage/internal/grpc_notification_metadata_parser.h"
#include "google/cloud/storage/internal/grpc_notification_request_parser.h"
#include "google/cloud/storage/internal/grpc_object_access_control_parser.h"
#include "google/cloud/storage/internal/grpc_object_metadata_parser.h"
#include "google/cloud/storage/internal/grpc_object_read_source.h"
#include "google/cloud/storage/internal/grpc_object_request_parser.h"
#include "google/cloud/storage/internal/grpc_service_account_parser.h"
#include "google/cloud/storage/internal/grpc_sign_blob_request_parser.h"
#include "google/cloud/storage/internal/grpc_split_write_object_data.h"
#include "google/cloud/storage/internal/grpc_synthetic_self_link.h"
#include "google/cloud/storage/internal/storage_stub_factory.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/big_endian.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/log.h"
#include "absl/strings/match.h"
#include <grpcpp/grpcpp.h>
#include <algorithm>
#include <cinttypes>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

template <typename AccessControl>
StatusOr<google::protobuf::RepeatedPtrField<AccessControl>> UpsertAcl(
    google::protobuf::RepeatedPtrField<AccessControl> acl,
    std::string const& entity, std::string const& role) {
  auto i =
      std::find_if(acl.begin(), acl.end(), [&](AccessControl const& entry) {
        return entry.entity() == entity || entry.entity_alt() == entity;
      });
  if (i != acl.end()) {
    i->set_role(role);
    return acl;
  }
  AccessControl entry;
  entry.set_entity(entity);
  entry.set_role(role);
  acl.Add(std::move(entry));
  return acl;
}

// Used in the implementation of `*BucketAcl()`.
StatusOr<storage::BucketAccessControl> FindBucketAccessControl(
    StatusOr<google::storage::v2::Bucket> response, std::string const& entity,
    std::string const& bucket_self_link) {
  if (!response) return std::move(response).status();
  for (auto const& acl : response->acl()) {
    if (acl.entity() != entity && acl.entity_alt() != entity) continue;
    return FromProto(acl, response->bucket_id(), bucket_self_link);
  }
  return Status(
      StatusCode::kNotFound,
      "cannot find entity <" + entity + "> in bucket " + response->bucket_id());
}

// Used in the implementation of `*ObjectAcl()`.
StatusOr<storage::ObjectAccessControl> FindObjectAccessControl(
    StatusOr<google::storage::v2::Object> response, std::string const& entity,
    std::string const& object_self_link) {
  if (!response) return std::move(response).status();
  auto bucket_id = GrpcBucketNameToId(response->bucket());
  for (auto const& acl : response->acl()) {
    if (acl.entity() != entity && acl.entity_alt() != entity) continue;
    return FromProto(acl, bucket_id, response->name(), response->generation(),
                     object_self_link);
  }
  return Status(StatusCode::kNotFound, "cannot find entity <" + entity +
                                           "> in bucket/object " + bucket_id +
                                           "/" + response->name());
}

// Used in the implementation of `*DefaultObjectAcl()`.
StatusOr<storage::ObjectAccessControl> FindDefaultObjectAccessControl(
    StatusOr<google::storage::v2::Bucket> response, std::string const& entity) {
  if (!response) return std::move(response).status();
  for (auto const& acl : response->default_object_acl()) {
    if (acl.entity() != entity && acl.entity_alt() != entity) continue;
    return FromProtoDefaultObjectAccessControl(acl, response->bucket_id());
  }
  return Status(
      StatusCode::kNotFound,
      "cannot find entity <" + entity + "> in bucket " + response->bucket_id());
}

// If this is the last `Write()` call of the last `InsertObjectMedia()` set the
// flags to finalize the request
void MaybeFinalize(
    google::storage::v2::WriteObjectRequest& write_request,
    grpc::WriteOptions& options,
    storage::internal::InsertObjectMediaRequest const& request,
    bool chunk_has_more) {
  write_request.set_finish_write(!chunk_has_more);
  if (chunk_has_more) return;
  options.set_last_message();

  // TODO(coryan) - return error if conversions to proto fail.
  auto hashes = request.hash_function().Finish();
  auto& checksums = *write_request.mutable_object_checksums();
  if (request.HasOption<storage::Crc32cChecksumValue>()) {
    // The client library accepts CRC32C checksums in the format required by the
    // REST APIs (base64-encoded big-endian, 32-bit integers). We need to
    // convert this to the format expected by proto, which is just a 32-bit
    // integer. But the value received by the application might be incorrect, so
    // we need to validate it.
    auto as_proto = storage_internal::Crc32cToProto(
        request.GetOption<storage::Crc32cChecksumValue>().value());
    if (as_proto) checksums.set_crc32c(*as_proto);
  } else if (!hashes.crc32c.empty()) {
    auto as_proto = storage_internal::Crc32cToProto(hashes.crc32c);
    if (as_proto) checksums.set_crc32c(*as_proto);
  }

  if (request.HasOption<storage::MD5HashValue>()) {
    auto as_proto = storage_internal::MD5ToProto(
        request.GetOption<storage::MD5HashValue>().value());
    if (as_proto) checksums.set_md5_hash(*std::move(as_proto));
  } else if (!hashes.md5.empty()) {
    auto as_proto = storage_internal::MD5ToProto(hashes.md5);
    if (as_proto) checksums.set_md5_hash(*as_proto);
  }
}

// If this is the last `Write()` call of the last `UploadChunk()` set the flags
// to finalize the request
void MaybeFinalize(google::storage::v2::WriteObjectRequest& write_request,
                   grpc::WriteOptions& options,
                   storage::internal::UploadChunkRequest const& request,
                   bool chunk_has_more) {
  if (!chunk_has_more) options.set_last_message();
  if (!request.last_chunk() || chunk_has_more) return;
  write_request.set_finish_write(true);
  auto hashes = request.hash_function().Finish();
  if (!hashes.md5.empty()) {
    auto md5 = MD5ToProto(hashes.md5);
    if (md5) {
      write_request.mutable_object_checksums()->set_md5_hash(*std::move(md5));
    }
  }
  if (!hashes.crc32c.empty()) {
    auto crc32c = Crc32cToProto(hashes.crc32c);
    if (crc32c) {
      write_request.mutable_object_checksums()->set_crc32c(*std::move(crc32c));
    }
  }
}

std::chrono::milliseconds ScaleStallTimeout(std::chrono::milliseconds timeout,
                                            std::uint32_t size,
                                            std::uint32_t quantum) {
  if (timeout == std::chrono::milliseconds(0)) return timeout;
  if (quantum <= size || size == 0) return timeout;
  return timeout * quantum / size;
}

Status TimeoutError(std::chrono::milliseconds timeout, std::string const& op) {
  return Status(StatusCode::kDeadlineExceeded,
                "timeout [" + absl::FormatDuration(absl::FromChrono(timeout)) +
                    "] while waiting for " + op);
}

StatusOr<storage::internal::QueryResumableUploadResponse>
HandleWriteObjectError(std::chrono::milliseconds timeout,
                       std::function<future<bool>()> const& create_watchdog,
                       std::unique_ptr<GrpcClient::WriteObjectStream> writer,
                       google::cloud::Options const& options) {
  auto watchdog = create_watchdog().then([&writer](auto f) {
    if (!f.get()) return false;
    writer->Cancel();
    return true;
  });
  auto close = writer->Close();
  watchdog.cancel();
  if (watchdog.get()) return TimeoutError(timeout, "Close()");
  if (!close) return std::move(close).status();
  return FromProto(*std::move(close), options, writer->GetRequestMetadata());
}

StatusOr<storage::internal::QueryResumableUploadResponse>
HandleUploadChunkError(std::chrono::milliseconds timeout,
                       std::function<future<bool>()> const& create_watchdog,
                       std::unique_ptr<GrpcClient::WriteObjectStream> writer,
                       google::cloud::Options const& options) {
  return HandleWriteObjectError(timeout, create_watchdog, std::move(writer),
                                options);
}

StatusOr<storage::ObjectMetadata> HandleInsertObjectMediaError(
    std::chrono::milliseconds timeout,
    std::function<future<bool>()> const& create_watchdog,
    std::unique_ptr<GrpcClient::WriteObjectStream> writer,
    google::cloud::Options const& options) {
  auto response = HandleWriteObjectError(timeout, create_watchdog,
                                         std::move(writer), options);
  if (!response) return std::move(response).status();
  if (response->payload) return std::move(*response->payload);
  return storage::ObjectMetadata{};
}

StatusOr<storage::internal::QueryResumableUploadResponse>
CloseWriteObjectStream(std::chrono::milliseconds timeout,
                       std::function<future<bool>()> const& create_watchdog,
                       std::unique_ptr<GrpcClient::WriteObjectStream> writer,
                       google::cloud::Options const& options) {
  auto watchdog = create_watchdog().then([&writer](auto f) {
    if (!f.get()) return false;
    writer->Cancel();
    return true;
  });
  auto response = writer->Close();
  watchdog.cancel();
  if (watchdog.get()) return TimeoutError(timeout, "Close()");
  if (!response) return std::move(response).status();
  return FromProto(*std::move(response), options, writer->GetRequestMetadata());
}

}  // namespace

using ::google::cloud::internal::CurrentOptions;
using ::google::cloud::internal::MakeBackgroundThreadsFactory;

int DefaultGrpcNumChannels(std::string const& endpoint) {
  // When using DirectPath the gRPC library already does load balancing across
  // multiple sockets, it makes little sense to perform additional load
  // balancing in the client library.
  if (absl::StartsWith(endpoint, "google-c2p:///") ||
      absl::StartsWith(endpoint, "google-c2p-experimental:///")) {
    return 1;
  }
  // When not using DirectPath, there are limits to the bandwidth per channel,
  // we want to create more channels to avoid hitting said limits.  The value
  // here is mostly a guess: we know 1 channel is too little for most
  // applications, but the ideal number depends on the workload.  The
  // application can always override this default, so it is not important to
  // have it exactly right.
  auto constexpr kMinimumChannels = 4;
  auto const count = std::thread::hardware_concurrency();
  return (std::max)(kMinimumChannels, static_cast<int>(count));
}

Options DefaultOptionsGrpc(Options options) {
  using ::google::cloud::internal::GetEnv;

  options =
      storage::internal::DefaultOptionsWithCredentials(std::move(options));
  if (!options.has<UnifiedCredentialsOption>() &&
      !options.has<GrpcCredentialOption>()) {
    options.set<UnifiedCredentialsOption>(
        google::cloud::MakeGoogleDefaultCredentials());
  }
  auto const testbench =
      GetEnv("CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT");
  if (testbench.has_value() && !testbench->empty()) {
    options.set<EndpointOption>(*testbench);
    // The emulator does not support HTTPS or authentication, use insecure
    // (sometimes called "anonymous") credentials, which disable SSL.
    options.set<UnifiedCredentialsOption>(MakeInsecureCredentials());
  }
  if (!options.has<EndpointOption>()) {
    options.set<EndpointOption>("storage.googleapis.com");
  }
  if (!options.has<AuthorityOption>()) {
    options.set<AuthorityOption>("storage.googleapis.com");
  }
  if (!options.has<GrpcNumChannelsOption>()) {
    options.set<GrpcNumChannelsOption>(
        DefaultGrpcNumChannels(options.get<EndpointOption>()));
  }
  return options;
}

std::shared_ptr<GrpcClient> GrpcClient::Create(Options opts) {
  // Cannot use std::make_shared<> as the constructor is private.
  return std::shared_ptr<GrpcClient>(new GrpcClient(std::move(opts)));
}

std::shared_ptr<GrpcClient> GrpcClient::CreateMock(
    std::shared_ptr<StorageStub> stub, Options opts) {
  return CreateMock(std::move(stub), {}, std::move(opts));
}

std::shared_ptr<GrpcClient> GrpcClient::CreateMock(
    std::shared_ptr<StorageStub> stub,
    std::shared_ptr<google::cloud::internal::MinimalIamCredentialsStub> iam,
    Options opts) {
  // Cannot use std::make_shared<> as the constructor is private.
  return std::shared_ptr<GrpcClient>(new GrpcClient(
      std::move(stub), std::move(iam), DefaultOptionsGrpc(std::move(opts))));
}

GrpcClient::GrpcClient(Options opts)
    : options_(std::move(opts)),
      backwards_compatibility_options_(
          storage::internal::MakeBackwardsCompatibleClientOptions(options_)),
      background_(MakeBackgroundThreadsFactory(options_)()),
      stub_(CreateStorageStub(background_->cq(), options_)),
      iam_stub_(CreateStorageIamStub(background_->cq(), options_)) {}

GrpcClient::GrpcClient(
    std::shared_ptr<StorageStub> stub,
    std::shared_ptr<google::cloud::internal::MinimalIamCredentialsStub> iam,
    Options opts)
    : options_(std::move(opts)),
      backwards_compatibility_options_(
          storage::internal::MakeBackwardsCompatibleClientOptions(options_)),
      background_(MakeBackgroundThreadsFactory(options_)()),
      stub_(std::move(stub)),
      iam_stub_(std::move(iam)) {}

storage::ClientOptions const& GrpcClient::client_options() const {
  return backwards_compatibility_options_;
}

Options GrpcClient::options() const { return options_; }

StatusOr<storage::internal::ListBucketsResponse> GrpcClient::ListBuckets(
    storage::internal::ListBucketsRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->ListBuckets(context, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::BucketMetadata> GrpcClient::CreateBucket(
    storage::internal::CreateBucketRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->CreateBucket(context, proto);
  if (response) return FromProto(*response, internal::CurrentOptions());
  // GCS returns kFailedPrecondition when the bucket already exists. I filed
  // a bug to change this to kAborted, for consistency with JSON.  In either
  // case, the error is confusing for customers. We normalize it here, just
  // as we do for the JSON transport.
  auto const code = response.status().code();
  if (code == StatusCode::kFailedPrecondition || code == StatusCode::kAborted) {
    return Status(StatusCode::kAlreadyExists, response.status().message(),
                  response.status().error_info());
  }
  return std::move(response).status();
}

StatusOr<storage::BucketMetadata> GrpcClient::GetBucketMetadata(
    storage::internal::GetBucketMetadataRequest const& request) {
  auto response = GetBucketMetadataImpl(request);
  if (!response) return std::move(response).status();
  return FromProto(*response, CurrentOptions());
}

StatusOr<storage::internal::EmptyResponse> GrpcClient::DeleteBucket(
    storage::internal::DeleteBucketRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto status = stub_->DeleteBucket(context, proto);
  if (!status.ok()) return status;
  return storage::internal::EmptyResponse{};
}

StatusOr<storage::BucketMetadata> GrpcClient::UpdateBucket(
    storage::internal::UpdateBucketRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->UpdateBucket(context, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response, internal::CurrentOptions());
}

StatusOr<storage::BucketMetadata> GrpcClient::PatchBucket(
    storage::internal::PatchBucketRequest const& request) {
  auto response = PatchBucketImpl(request);
  if (!response) return std::move(response).status();
  return FromProto(*response, CurrentOptions());
}

StatusOr<storage::NativeIamPolicy> GrpcClient::GetNativeBucketIamPolicy(
    storage::internal::GetBucketIamPolicyRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->GetIamPolicy(context, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::NativeIamPolicy> GrpcClient::SetNativeBucketIamPolicy(
    storage::internal::SetNativeBucketIamPolicyRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->SetIamPolicy(context, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::internal::TestBucketIamPermissionsResponse>
GrpcClient::TestBucketIamPermissions(
    storage::internal::TestBucketIamPermissionsRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->TestIamPermissions(context, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::BucketMetadata> GrpcClient::LockBucketRetentionPolicy(
    storage::internal::LockBucketRetentionPolicyRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->LockBucketRetentionPolicy(context, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response, CurrentOptions());
}

StatusOr<storage::ObjectMetadata> GrpcClient::InsertObjectMedia(
    storage::internal::InsertObjectMediaRequest const& request) {
  auto r = ToProto(request);
  if (!r) return std::move(r).status();
  auto proto_request = *std::move(r);

  auto const& current = google::cloud::internal::CurrentOptions();
  auto timeout = ScaleStallTimeout(
      current.get<storage::TransferStallTimeoutOption>(),
      current.get<storage::TransferStallMinimumRateOption>(),
      google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES);

  auto create_watchdog = [cq = background_->cq(), timeout]() mutable {
    if (timeout == std::chrono::seconds(0)) {
      return make_ready_future(false);
    }
    return cq.MakeRelativeTimer(timeout).then(
        [](auto f) { return f.get().ok(); });
  };

  auto context = std::make_shared<grpc::ClientContext>();
  // The REST response is just the object metadata (aka the "resource"). In the
  // gRPC response the object metadata is in a "resource" field. Passing an
  // extra prefix to ApplyQueryParameters sends the right
  // filtering instructions to the gRPC API.
  ApplyQueryParameters(*context, request, "resource");
  ApplyRoutingHeaders(*context, request);
  auto stream = stub_->WriteObject(std::move(context));

  using ContentType = std::remove_const_t<std::remove_reference_t<
      decltype(std::declval<google::storage::v2::ChecksummedData>()
                   .content())>>;
  auto splitter = SplitObjectWriteData<ContentType>(request.payload());
  std::int64_t offset = 0;

  // This loop must run at least once because we need to send at least one
  // Write() call for empty objects.
  do {
    proto_request.set_write_offset(offset);
    auto& data = *proto_request.mutable_checksummed_data();
    data.set_content(splitter.Next());
    data.set_crc32c(Crc32c(data.content()));
    request.hash_function().Update(offset, data.content(), data.crc32c());
    offset += data.content().size();

    auto options = grpc::WriteOptions{};
    MaybeFinalize(proto_request, options, request, !splitter.Done());
    auto watchdog = create_watchdog().then([&stream](auto f) {
      if (!f.get()) return false;
      stream->Cancel();
      return true;
    });
    auto success = stream->Write(proto_request, options);
    watchdog.cancel();
    if (watchdog.get()) {
      // The stream is cancelled by the watchdog. We still need to close it.
      (void)stream->Close();
      stream.reset();
      return TimeoutError(timeout, "Write()");
    }
    if (!success) {
      return HandleInsertObjectMediaError(timeout, create_watchdog,
                                          std::move(stream), current);
    }
    // After the first message, clear the object specification and checksums,
    // there is no need to resend it.
    proto_request.clear_write_object_spec();
    proto_request.clear_upload_id();
    proto_request.clear_object_checksums();
  } while (!splitter.Done());
  auto response = CloseWriteObjectStream(timeout, create_watchdog,
                                         std::move(stream), current);
  if (!response) return std::move(response).status();
  if (response->payload) return std::move(*response->payload);
  return storage::ObjectMetadata{};
}

StatusOr<storage::ObjectMetadata> GrpcClient::CopyObject(
    storage::internal::CopyObjectRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request, "resource");
  auto response = stub_->RewriteObject(context, *proto);
  if (!response) return std::move(response).status();
  if (!response->done()) {
    return Status(
        StatusCode::kOutOfRange,
        "Object too large, use RewriteObject() instead of CopyObject()");
  }
  return FromProto(response->resource(), CurrentOptions());
}

StatusOr<storage::ObjectMetadata> GrpcClient::GetObjectMetadata(
    storage::internal::GetObjectMetadataRequest const& request) {
  auto response = GetObjectMetadataImpl(request);
  if (!response) return std::move(response).status();
  return FromProto(*response, CurrentOptions());
}

StatusOr<std::unique_ptr<storage::internal::ObjectReadSource>>
GrpcClient::ReadObject(
    storage::internal::ReadObjectRangeRequest const& request) {
  // With the REST API this condition was detected by the server as an error,
  // generally we prefer the server to detect errors because its answers are
  // authoritative. In this case, the server cannot: with gRPC '0' is the same
  // as "not set" and the server would send back the full file, which was
  // unlikely to be the customer's intent.
  if (request.HasOption<storage::ReadLast>() &&
      request.GetOption<storage::ReadLast>().value() == 0) {
    return Status(
        StatusCode::kOutOfRange,
        "ReadLast(0) is invalid in REST and produces incorrect output in gRPC");
  }
  auto context = std::make_shared<grpc::ClientContext>();
  ApplyQueryParameters(*context, request);
  auto proto_request = ToProto(request);
  if (!proto_request) return std::move(proto_request).status();
  auto stream = stub_->ReadObject(std::move(context), *proto_request);

  // The default timer source is a no-op. It does not set a timer, and always
  // returns an indication that the timer expired.  The GrpcObjectReadSource
  // takes no action on expired timers.
  GrpcObjectReadSource::TimerSource timer_source = [] {
    return make_ready_future(false);
  };
  auto const timeout =
      CurrentOptions().get<storage::DownloadStallTimeoutOption>();
  if (timeout != std::chrono::seconds(0)) {
    // Change to an active timer.
    timer_source = [timeout, cq = background_->cq()]() mutable {
      return cq.MakeRelativeTimer(timeout).then(
          [](auto f) { return f.get().ok(); });
    };
  }

  return std::unique_ptr<storage::internal::ObjectReadSource>(
      absl::make_unique<GrpcObjectReadSource>(std::move(timer_source),
                                              std::move(stream)));
}

StatusOr<storage::internal::ListObjectsResponse> GrpcClient::ListObjects(
    storage::internal::ListObjectsRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->ListObjects(context, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response, CurrentOptions());
}

StatusOr<storage::internal::EmptyResponse> GrpcClient::DeleteObject(
    storage::internal::DeleteObjectRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->DeleteObject(context, proto);
  if (!response.ok()) return response;
  return storage::internal::EmptyResponse{};
}

StatusOr<storage::ObjectMetadata> GrpcClient::UpdateObject(
    storage::internal::UpdateObjectRequest const& request) {
  auto proto = ToProto(request);
  if (!proto) return std::move(proto).status();
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->UpdateObject(context, *proto);
  if (!response) return std::move(response).status();
  return FromProto(*response, CurrentOptions());
}

StatusOr<storage::ObjectMetadata> GrpcClient::PatchObject(
    storage::internal::PatchObjectRequest const& request) {
  auto response = PatchObjectImpl(request);
  if (!response) return std::move(response).status();
  return FromProto(*response, CurrentOptions());
}

StatusOr<storage::ObjectMetadata> GrpcClient::ComposeObject(
    storage::internal::ComposeObjectRequest const& request) {
  auto proto = ToProto(request);
  if (!proto) return std::move(proto).status();
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->ComposeObject(context, *proto);
  if (!response) return std::move(response).status();
  return FromProto(*response, CurrentOptions());
}

StatusOr<storage::internal::RewriteObjectResponse> GrpcClient::RewriteObject(
    storage::internal::RewriteObjectRequest const& request) {
  auto proto = ToProto(request);
  if (!proto) return std::move(proto).status();
  grpc::ClientContext context;
  ApplyQueryParameters(context, request, "resource");
  auto response = stub_->RewriteObject(context, *proto);
  if (!response) return std::move(response).status();
  return FromProto(*response, CurrentOptions());
}

StatusOr<storage::internal::CreateResumableUploadResponse>
GrpcClient::CreateResumableUpload(
    storage::internal::ResumableUploadRequest const& request) {
  auto proto_request = ToProto(request);
  if (!proto_request) return std::move(proto_request).status();

  grpc::ClientContext context;
  ApplyQueryParameters(context, request, "resource");
  auto const timeout =
      CurrentOptions().get<storage::TransferStallTimeoutOption>();
  if (timeout.count() != 0) {
    context.set_deadline(std::chrono::system_clock::now() + timeout);
  }
  auto response = stub_->StartResumableWrite(context, *proto_request);
  if (!response.ok()) return std::move(response).status();

  return storage::internal::CreateResumableUploadResponse{
      response->upload_id()};
}

StatusOr<storage::internal::QueryResumableUploadResponse>
GrpcClient::QueryResumableUpload(
    storage::internal::QueryResumableUploadRequest const& request) {
  grpc::ClientContext context;
  ApplyQueryParameters(context, request, "resource");
  auto const timeout =
      CurrentOptions().get<storage::TransferStallTimeoutOption>();
  if (timeout.count() != 0) {
    context.set_deadline(std::chrono::system_clock::now() + timeout);
  }
  auto response = stub_->QueryWriteStatus(context, ToProto(request));
  if (!response) return std::move(response).status();
  return FromProto(*response, CurrentOptions());
}

StatusOr<storage::internal::EmptyResponse> GrpcClient::DeleteResumableUpload(
    storage::internal::DeleteResumableUploadRequest const& request) {
  grpc::ClientContext context;
  ApplyQueryParameters(context, request, "");
  auto const timeout =
      CurrentOptions().get<storage::TransferStallTimeoutOption>();
  if (timeout.count() != 0) {
    context.set_deadline(std::chrono::system_clock::now() + timeout);
  }
  auto response = stub_->CancelResumableWrite(context, ToProto(request));
  if (!response) return std::move(response).status();
  return storage::internal::EmptyResponse{};
}

StatusOr<storage::internal::QueryResumableUploadResponse>
GrpcClient::UploadChunk(storage::internal::UploadChunkRequest const& request) {
  auto proto_request = google::storage::v2::WriteObjectRequest{};
  proto_request.set_upload_id(request.upload_session_url());

  auto const& current = google::cloud::internal::CurrentOptions();
  auto const timeout = ScaleStallTimeout(
      current.get<storage::TransferStallTimeoutOption>(),
      current.get<storage::TransferStallMinimumRateOption>(),
      google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES);

  auto create_watchdog = [cq = background_->cq(), timeout]() mutable {
    if (timeout == std::chrono::seconds(0)) {
      return make_ready_future(false);
    }
    return cq.MakeRelativeTimer(timeout).then(
        [](auto f) { return f.get().ok(); });
  };

  auto context = std::make_shared<grpc::ClientContext>();
  // The REST response is just the object metadata (aka the "resource"). In the
  // gRPC response the object metadata is in a "resource" field. Passing an
  // extra prefix to ApplyQueryParameters sends the right
  // filtering instructions to the gRPC API.
  ApplyQueryParameters(*context, request, "resource");
  ApplyRoutingHeaders(*context, request);
  auto stream = stub_->WriteObject(std::move(context));

  using ContentType = std::remove_const_t<std::remove_reference_t<
      decltype(std::declval<google::storage::v2::ChecksummedData>()
                   .content())>>;
  auto splitter = SplitObjectWriteData<ContentType>(request.payload());
  auto offset = request.offset();

  // This loop must run at least once because we need to send at least one
  // Write() call for empty objects.
  do {
    proto_request.set_write_offset(offset);
    auto& data = *proto_request.mutable_checksummed_data();
    data.set_content(splitter.Next());
    data.set_crc32c(Crc32c(data.content()));
    request.hash_function().Update(offset, data.content(), data.crc32c());
    offset += data.content().size();

    auto options = grpc::WriteOptions();
    MaybeFinalize(proto_request, options, request, !splitter.Done());

    auto watchdog = create_watchdog().then([&stream](auto f) {
      if (!f.get()) return false;
      stream->Cancel();
      return true;
    });
    auto success = stream->Write(proto_request, options);
    watchdog.cancel();
    if (watchdog.get()) {
      // The stream is cancelled by the watchdog. We still need to close it.
      (void)stream->Close();
      stream.reset();
      return TimeoutError(timeout, "Write()");
    }
    if (!success) {
      return HandleUploadChunkError(timeout, create_watchdog, std::move(stream),
                                    current);
    }
    // After the first message, clear the object specification and checksums,
    // there is no need to resend it.
    proto_request.clear_write_object_spec();
    proto_request.clear_upload_id();
    proto_request.clear_object_checksums();
  } while (!splitter.Done());
  return CloseWriteObjectStream(timeout, create_watchdog, std::move(stream),
                                current);
}

StatusOr<storage::internal::ListBucketAclResponse> GrpcClient::ListBucketAcl(
    storage::internal::ListBucketAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto get = GetBucketMetadata(get_request);
  if (!get) return std::move(get).status();
  storage::internal::ListBucketAclResponse response;
  response.items = get->acl();
  return response;
}

StatusOr<storage::BucketAccessControl> GrpcClient::GetBucketAcl(
    storage::internal::GetBucketAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto get = GetBucketMetadataImpl(get_request);
  auto const bucket_self_link =
      SyntheticSelfLinkBucket(CurrentOptions(), request.bucket_name());
  return FindBucketAccessControl(std::move(get), request.entity(),
                                 bucket_self_link);
}

StatusOr<storage::BucketAccessControl> GrpcClient::CreateBucketAcl(
    storage::internal::CreateBucketAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_multiple_options(storage::Projection("full"),
                                   storage::Fields());
  auto updater = [&request](BucketAccessControlList acl) {
    return UpsertAcl(std::move(acl), request.entity(), request.role());
  };
  auto const bucket_self_link =
      SyntheticSelfLinkBucket(CurrentOptions(), request.bucket_name());
  return FindBucketAccessControl(
      ModifyBucketAccessControl(get_request, updater), request.entity(),
      bucket_self_link);
}

StatusOr<storage::internal::EmptyResponse> GrpcClient::DeleteBucketAcl(
    storage::internal::DeleteBucketAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_multiple_options(storage::Projection("full"),
                                   storage::Fields());
  auto updater =
      [&request](
          BucketAccessControlList acl) -> StatusOr<BucketAccessControlList> {
    auto i = std::remove_if(
        acl.begin(), acl.end(), [&entity = request.entity()](auto const& a) {
          return a.entity() == entity || a.entity_alt() == entity;
        });
    if (i == acl.end()) {
      return Status(StatusCode::kNotFound,
                    "the entity <" + request.entity() +
                        "> is not present in the ACL for bucket " +
                        request.bucket_name());
    }
    acl.erase(i, acl.end());
    return acl;
  };
  auto response = ModifyBucketAccessControl(get_request, updater);
  if (!response) return std::move(response).status();
  return storage::internal::EmptyResponse{};
}

StatusOr<storage::BucketAccessControl> GrpcClient::UpdateBucketAcl(
    storage::internal::UpdateBucketAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_multiple_options(storage::Projection("full"),
                                   storage::Fields());
  auto updater = [&request](BucketAccessControlList acl) {
    return UpsertAcl(std::move(acl), request.entity(), request.role());
  };
  auto const bucket_self_link =
      SyntheticSelfLinkBucket(CurrentOptions(), request.bucket_name());
  return FindBucketAccessControl(
      ModifyBucketAccessControl(get_request, updater), request.entity(),
      bucket_self_link);
}

StatusOr<storage::BucketAccessControl> GrpcClient::PatchBucketAcl(
    storage::internal::PatchBucketAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_multiple_options(storage::Projection("full"));
  auto updater = [&request](BucketAccessControlList acl) {
    return UpsertAcl(std::move(acl), request.entity(), Role(request.patch()));
  };
  auto const bucket_self_link =
      SyntheticSelfLinkBucket(CurrentOptions(), request.bucket_name());
  return FindBucketAccessControl(
      ModifyBucketAccessControl(get_request, updater), request.entity(),
      bucket_self_link);
}

StatusOr<storage::internal::ListObjectAclResponse> GrpcClient::ListObjectAcl(
    storage::internal::ListObjectAclRequest const& request) {
  auto get_request = storage::internal::GetObjectMetadataRequest(
      request.bucket_name(), request.object_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto get = GetObjectMetadata(get_request);
  if (!get) return std::move(get).status();
  storage::internal::ListObjectAclResponse response;
  response.items = get->acl();
  return response;
}

StatusOr<storage::ObjectAccessControl> GrpcClient::CreateObjectAcl(
    storage::internal::CreateObjectAclRequest const& request) {
  auto get_request = storage::internal::GetObjectMetadataRequest(
      request.bucket_name(), request.object_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto updater = [&request](ObjectAccessControlList acl) {
    return UpsertAcl(std::move(acl), request.entity(), request.role());
  };
  auto const object_self_link = SyntheticSelfLinkObject(
      CurrentOptions(), request.bucket_name(), request.object_name());
  return FindObjectAccessControl(
      ModifyObjectAccessControl(get_request, updater), request.entity(),
      object_self_link);
}

StatusOr<storage::internal::EmptyResponse> GrpcClient::DeleteObjectAcl(
    storage::internal::DeleteObjectAclRequest const& request) {
  auto get_request = storage::internal::GetObjectMetadataRequest(
      request.bucket_name(), request.object_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto updater =
      [&request](
          ObjectAccessControlList acl) -> StatusOr<ObjectAccessControlList> {
    auto i = std::remove_if(
        acl.begin(), acl.end(), [&entity = request.entity()](auto const& a) {
          return a.entity() == entity || a.entity_alt() == entity;
        });
    if (i == acl.end()) {
      return Status(StatusCode::kNotFound,
                    "the entity <" + request.entity() +
                        "> is not present in the ACL for object " +
                        request.object_name());
    }
    acl.erase(i, acl.end());
    return acl;
  };
  auto response = ModifyObjectAccessControl(get_request, updater);
  if (!response) return std::move(response).status();
  return storage::internal::EmptyResponse{};
}

StatusOr<storage::ObjectAccessControl> GrpcClient::GetObjectAcl(
    storage::internal::GetObjectAclRequest const& request) {
  auto get_request = storage::internal::GetObjectMetadataRequest(
      request.bucket_name(), request.object_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto get = GetObjectMetadataImpl(get_request);
  auto const object_self_link = SyntheticSelfLinkObject(
      CurrentOptions(), request.bucket_name(), request.object_name());
  return FindObjectAccessControl(std::move(get), request.entity(),
                                 object_self_link);
}

StatusOr<storage::ObjectAccessControl> GrpcClient::UpdateObjectAcl(
    storage::internal::UpdateObjectAclRequest const& request) {
  auto get_request = storage::internal::GetObjectMetadataRequest(
      request.bucket_name(), request.object_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto updater = [&request](ObjectAccessControlList acl) {
    return UpsertAcl(std::move(acl), request.entity(), request.role());
  };
  auto const object_self_link = SyntheticSelfLinkObject(
      CurrentOptions(), request.bucket_name(), request.object_name());
  return FindObjectAccessControl(
      ModifyObjectAccessControl(get_request, updater), request.entity(),
      object_self_link);
}

StatusOr<storage::ObjectAccessControl> GrpcClient::PatchObjectAcl(
    storage::internal::PatchObjectAclRequest const& request) {
  auto get_request = storage::internal::GetObjectMetadataRequest(
      request.bucket_name(), request.object_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto updater = [&request](ObjectAccessControlList acl) {
    return UpsertAcl(std::move(acl), request.entity(), Role(request.patch()));
  };
  auto const object_self_link = SyntheticSelfLinkObject(
      CurrentOptions(), request.bucket_name(), request.object_name());
  return FindObjectAccessControl(
      ModifyObjectAccessControl(get_request, updater), request.entity(),
      object_self_link);
}

StatusOr<storage::internal::ListDefaultObjectAclResponse>
GrpcClient::ListDefaultObjectAcl(
    storage::internal::ListDefaultObjectAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto get = GetBucketMetadata(get_request);
  if (!get) return std::move(get).status();
  storage::internal::ListDefaultObjectAclResponse response;
  response.items = get->default_acl();
  return response;
}

StatusOr<storage::ObjectAccessControl> GrpcClient::CreateDefaultObjectAcl(
    storage::internal::CreateDefaultObjectAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto updater = [&request](ObjectAccessControlList acl) {
    return UpsertAcl(std::move(acl), request.entity(), request.role());
  };
  return FindDefaultObjectAccessControl(
      ModifyDefaultAccessControl(get_request, updater), request.entity());
}

StatusOr<storage::internal::EmptyResponse> GrpcClient::DeleteDefaultObjectAcl(
    storage::internal::DeleteDefaultObjectAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto updater =
      [&request](
          ObjectAccessControlList acl) -> StatusOr<ObjectAccessControlList> {
    auto i = std::remove_if(
        acl.begin(), acl.end(), [&entity = request.entity()](auto const& a) {
          return a.entity() == entity || a.entity_alt() == entity;
        });
    if (i == acl.end()) {
      return Status(StatusCode::kNotFound,
                    "the entity <" + request.entity() +
                        "> is not present in the ACL for bucket " +
                        request.bucket_name());
    }
    acl.erase(i, acl.end());
    return acl;
  };
  auto response = ModifyDefaultAccessControl(get_request, updater);
  if (!response) return std::move(response).status();
  return storage::internal::EmptyResponse{};
}

StatusOr<storage::ObjectAccessControl> GrpcClient::GetDefaultObjectAcl(
    storage::internal::GetDefaultObjectAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto get = GetBucketMetadataImpl(get_request);
  return FindDefaultObjectAccessControl(std::move(get), request.entity());
}

StatusOr<storage::ObjectAccessControl> GrpcClient::UpdateDefaultObjectAcl(
    storage::internal::UpdateDefaultObjectAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto updater = [&request](ObjectAccessControlList acl) {
    return UpsertAcl(std::move(acl), request.entity(), request.role());
  };
  return FindDefaultObjectAccessControl(
      ModifyDefaultAccessControl(get_request, updater), request.entity());
}

StatusOr<storage::ObjectAccessControl> GrpcClient::PatchDefaultObjectAcl(
    storage::internal::PatchDefaultObjectAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto updater = [&request](ObjectAccessControlList acl) {
    return UpsertAcl(std::move(acl), request.entity(), Role(request.patch()));
  };
  return FindDefaultObjectAccessControl(
      ModifyDefaultAccessControl(get_request, updater), request.entity());
}

StatusOr<storage::ServiceAccount> GrpcClient::GetServiceAccount(
    storage::internal::GetProjectServiceAccountRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->GetServiceAccount(context, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::internal::ListHmacKeysResponse> GrpcClient::ListHmacKeys(
    storage::internal::ListHmacKeysRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->ListHmacKeys(context, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::internal::CreateHmacKeyResponse> GrpcClient::CreateHmacKey(
    storage::internal::CreateHmacKeyRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->CreateHmacKey(context, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::internal::EmptyResponse> GrpcClient::DeleteHmacKey(
    storage::internal::DeleteHmacKeyRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->DeleteHmacKey(context, proto);
  if (!response.ok()) return response;
  return storage::internal::EmptyResponse{};
}

StatusOr<storage::HmacKeyMetadata> GrpcClient::GetHmacKey(
    storage::internal::GetHmacKeyRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->GetHmacKey(context, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::HmacKeyMetadata> GrpcClient::UpdateHmacKey(
    storage::internal::UpdateHmacKeyRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->UpdateHmacKey(context, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::internal::SignBlobResponse> GrpcClient::SignBlob(
    storage::internal::SignBlobRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  // This request does not have any options that require using
  //     ApplyQueryParameters(context, request)
  auto response = iam_stub_->SignBlob(context, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::internal::ListNotificationsResponse>
GrpcClient::ListNotifications(
    storage::internal::ListNotificationsRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->ListNotificationConfigs(context, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::NotificationMetadata> GrpcClient::CreateNotification(
    storage::internal::CreateNotificationRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->CreateNotificationConfig(context, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::NotificationMetadata> GrpcClient::GetNotification(
    storage::internal::GetNotificationRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->GetNotificationConfig(context, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::internal::EmptyResponse> GrpcClient::DeleteNotification(
    storage::internal::DeleteNotificationRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->DeleteNotificationConfig(context, proto);
  if (!response.ok()) return response;
  return storage::internal::EmptyResponse{};
}

StatusOr<google::storage::v2::Bucket> GrpcClient::GetBucketMetadataImpl(
    storage::internal::GetBucketMetadataRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  return stub_->GetBucket(context, proto);
}

StatusOr<google::storage::v2::Bucket> GrpcClient::PatchBucketImpl(
    storage::internal::PatchBucketRequest const& request) {
  auto proto = ToProto(request);
  if (!proto) return std::move(proto).status();
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  return stub_->UpdateBucket(context, *proto);
}

StatusOr<google::storage::v2::Object> GrpcClient::GetObjectMetadataImpl(
    storage::internal::GetObjectMetadataRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  return stub_->GetObject(context, proto);
}

StatusOr<google::storage::v2::Object> GrpcClient::PatchObjectImpl(
    storage::internal::PatchObjectRequest const& request) {
  auto proto = ToProto(request);
  if (!proto) return std::move(proto).status();
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  return stub_->UpdateObject(context, *proto);
}

StatusOr<google::storage::v2::Bucket> GrpcClient::ModifyBucketAccessControl(
    storage::internal::GetBucketMetadataRequest const& request,
    BucketAclUpdater const& updater) {
  auto response = GetBucketMetadataImpl(request);
  if (!response) return std::move(response).status();
  auto acl = updater(std::move(*response->mutable_acl()));
  if (!acl) return std::move(acl).status();

  auto const bucket_self_link =
      SyntheticSelfLinkBucket(CurrentOptions(), request.bucket_name());
  std::vector<storage::BucketAccessControl> updated(acl->size());
  std::transform(acl->begin(), acl->end(), updated.begin(),
                 [&request, &bucket_self_link](auto const& p) {
                   return FromProto(p, request.bucket_name(), bucket_self_link);
                 });
  auto patch_request = storage::internal::PatchBucketRequest(
      request.bucket_name(),
      storage::BucketMetadataPatchBuilder().SetAcl(std::move(updated)));
  request.ForEachOption(CopyCommonOptions(patch_request));
  patch_request.set_option(
      storage::IfMetagenerationMatch(response->metageneration()));
  auto patch = PatchBucketImpl(patch_request);
  // Retry on failed preconditions
  if (patch.status().code() == StatusCode::kFailedPrecondition) {
    return Status(
        StatusCode::kUnavailable,
        "retrying BucketAccessControl change due to conflict, bucket=" +
            request.bucket_name());
  }
  return patch;
}

StatusOr<google::storage::v2::Object> GrpcClient::ModifyObjectAccessControl(
    storage::internal::GetObjectMetadataRequest const& request,
    ObjectAclUpdater const& updater) {
  auto response = GetObjectMetadataImpl(request);
  if (!response) return std::move(response).status();
  auto acl = updater(std::move(*response->mutable_acl()));
  if (!acl) return std::move(acl).status();

  std::vector<storage::ObjectAccessControl> updated(acl->size());
  auto const object_self_link = SyntheticSelfLinkObject(
      CurrentOptions(), request.bucket_name(), request.object_name());
  std::transform(acl->begin(), acl->end(), updated.begin(), [&](auto const& p) {
    return FromProto(p, request.bucket_name(), response->name(),
                     response->generation(), object_self_link);
  });
  auto patch_request = storage::internal::PatchObjectRequest(
      request.bucket_name(), request.object_name(),
      storage::ObjectMetadataPatchBuilder().SetAcl(std::move(updated)));
  request.ForEachOption(CopyCommonOptions(patch_request));
  patch_request.set_multiple_options(
      storage::Generation(response->generation()),
      storage::IfMetagenerationMatch(response->metageneration()));
  auto patch = PatchObjectImpl(patch_request);
  // Retry on failed preconditions
  if (patch.status().code() == StatusCode::kFailedPrecondition) {
    return Status(
        StatusCode::kUnavailable,
        "retrying ObjectAccessControl change due to conflict, bucket=" +
            request.bucket_name() + ", object=" + request.object_name());
  }
  return patch;
}

StatusOr<google::storage::v2::Bucket> GrpcClient::ModifyDefaultAccessControl(
    storage::internal::GetBucketMetadataRequest const& request,
    DefaultObjectAclUpdater const& updater) {
  auto response = GetBucketMetadataImpl(request);
  if (!response) return std::move(response).status();
  auto acl = updater(std::move(*response->mutable_default_object_acl()));
  if (!acl) return std::move(acl).status();

  std::vector<storage::ObjectAccessControl> updated(acl->size());
  std::transform(acl->begin(), acl->end(), updated.begin(), [&](auto const& p) {
    return FromProtoDefaultObjectAccessControl(p, request.bucket_name());
  });

  auto patch_request = storage::internal::PatchBucketRequest(
      request.bucket_name(),
      storage::BucketMetadataPatchBuilder().SetDefaultAcl(std::move(updated)));
  request.ForEachOption(CopyCommonOptions(patch_request));
  patch_request.set_option(
      storage::IfMetagenerationMatch(response->metageneration()));
  auto patch = PatchBucketImpl(patch_request);
  // Retry on failed preconditions
  if (patch.status().code() == StatusCode::kFailedPrecondition) {
    return Status(
        StatusCode::kUnavailable,
        "retrying BucketAccessControl change due to conflict, bucket=" +
            request.bucket_name());
  }
  return *patch;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
