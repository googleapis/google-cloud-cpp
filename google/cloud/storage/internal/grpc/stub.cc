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

#include "google/cloud/storage/internal/grpc/stub.h"
#include "google/cloud/storage/internal/crc32c.h"
#include "google/cloud/storage/internal/grpc/bucket_access_control_parser.h"
#include "google/cloud/storage/internal/grpc/bucket_metadata_parser.h"
#include "google/cloud/storage/internal/grpc/bucket_name.h"
#include "google/cloud/storage/internal/grpc/bucket_request_parser.h"
#include "google/cloud/storage/internal/grpc/configure_client_context.h"
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/storage/internal/grpc/default_options.h"
#include "google/cloud/storage/internal/grpc/hmac_key_metadata_parser.h"
#include "google/cloud/storage/internal/grpc/hmac_key_request_parser.h"
#include "google/cloud/storage/internal/grpc/notification_metadata_parser.h"
#include "google/cloud/storage/internal/grpc/notification_request_parser.h"
#include "google/cloud/storage/internal/grpc/object_access_control_parser.h"
#include "google/cloud/storage/internal/grpc/object_metadata_parser.h"
#include "google/cloud/storage/internal/grpc/object_read_source.h"
#include "google/cloud/storage/internal/grpc/object_request_parser.h"
#include "google/cloud/storage/internal/grpc/scale_stall_timeout.h"
#include "google/cloud/storage/internal/grpc/service_account_parser.h"
#include "google/cloud/storage/internal/grpc/sign_blob_request_parser.h"
#include "google/cloud/storage/internal/grpc/split_write_object_data.h"
#include "google/cloud/storage/internal/grpc/synthetic_self_link.h"
#include "google/cloud/storage/internal/storage_stub_factory.h"
#include "google/cloud/internal/big_endian.h"
#include "google/cloud/internal/invoke_result.h"
#include "absl/strings/match.h"
#include "absl/time/time.h"
#include <grpcpp/grpcpp.h>
#include <algorithm>
#include <cinttypes>
#include <utility>

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

Status TimeoutError(std::chrono::milliseconds timeout, std::string const& op) {
  return Status(StatusCode::kDeadlineExceeded,
                "timeout [" + absl::FormatDuration(absl::FromChrono(timeout)) +
                    "] while waiting for " + op);
}

StatusOr<storage::internal::QueryResumableUploadResponse>
HandleWriteObjectError(std::chrono::milliseconds timeout,
                       std::function<future<bool>()> const& create_watchdog,
                       std::unique_ptr<GrpcStub::WriteObjectStream> writer,
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
                       std::unique_ptr<GrpcStub::WriteObjectStream> writer,
                       google::cloud::Options const& options) {
  return HandleWriteObjectError(timeout, create_watchdog, std::move(writer),
                                options);
}

StatusOr<storage::ObjectMetadata> HandleInsertObjectMediaError(
    std::chrono::milliseconds timeout,
    std::function<future<bool>()> const& create_watchdog,
    std::unique_ptr<GrpcStub::WriteObjectStream> writer,
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
                       std::unique_ptr<GrpcStub::WriteObjectStream> writer,
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

using ::google::cloud::internal::MakeBackgroundThreadsFactory;

GrpcStub::GrpcStub(Options opts)
    : options_(std::move(opts)),
      background_(MakeBackgroundThreadsFactory(options_)()),
      iam_stub_(CreateStorageIamStub(background_->cq(), options_)) {
  std::tie(refresh_, stub_) = CreateStorageStub(background_->cq(), options_);
}

GrpcStub::GrpcStub(
    std::shared_ptr<StorageStub> stub,
    std::shared_ptr<google::cloud::internal::MinimalIamCredentialsStub> iam,
    Options opts)
    : options_(std::move(opts)),
      background_(MakeBackgroundThreadsFactory(options_)()),
      stub_(std::move(stub)),
      iam_stub_(std::move(iam)) {}

Options GrpcStub::options() const { return options_; }

StatusOr<storage::internal::ListBucketsResponse> GrpcStub::ListBuckets(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::ListBucketsRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->ListBuckets(ctx, options, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::BucketMetadata> GrpcStub::CreateBucket(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::CreateBucketRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->CreateBucket(ctx, options, proto);
  if (response) return FromProto(*response, options);
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

StatusOr<storage::BucketMetadata> GrpcStub::GetBucketMetadata(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetBucketMetadataRequest const& request) {
  auto response = GetBucketMetadataImpl(context, options, request);
  if (!response) return std::move(response).status();
  return FromProto(*response, options);
}

StatusOr<storage::internal::EmptyResponse> GrpcStub::DeleteBucket(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::DeleteBucketRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto status = stub_->DeleteBucket(ctx, options, proto);
  if (!status.ok()) return status;
  return storage::internal::EmptyResponse{};
}

StatusOr<storage::BucketMetadata> GrpcStub::UpdateBucket(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::UpdateBucketRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->UpdateBucket(ctx, options, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response, options);
}

StatusOr<storage::BucketMetadata> GrpcStub::PatchBucket(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::PatchBucketRequest const& request) {
  auto response = PatchBucketImpl(context, options, request);
  if (!response) return std::move(response).status();
  return FromProto(*response, options);
}

StatusOr<storage::NativeIamPolicy> GrpcStub::GetNativeBucketIamPolicy(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetBucketIamPolicyRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->GetIamPolicy(ctx, options, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::NativeIamPolicy> GrpcStub::SetNativeBucketIamPolicy(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::SetNativeBucketIamPolicyRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->SetIamPolicy(ctx, options, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::internal::TestBucketIamPermissionsResponse>
GrpcStub::TestBucketIamPermissions(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::TestBucketIamPermissionsRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->TestIamPermissions(ctx, options, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::BucketMetadata> GrpcStub::LockBucketRetentionPolicy(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::LockBucketRetentionPolicyRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->LockBucketRetentionPolicy(ctx, options, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response, options);
}

StatusOr<storage::ObjectMetadata> GrpcStub::InsertObjectMedia(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::InsertObjectMediaRequest const& request) {
  auto r = ToProto(request);
  if (!r) return std::move(r).status();
  auto proto_request = *std::move(r);

  auto timeout = ScaleStallTimeout(
      options.get<storage::TransferStallTimeoutOption>(),
      options.get<storage::TransferStallMinimumRateOption>(),
      google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES);

  auto create_watchdog = [cq = background_->cq(), timeout]() mutable {
    if (timeout == std::chrono::seconds(0)) {
      return make_ready_future(false);
    }
    return cq.MakeRelativeTimer(timeout).then(
        [](auto f) { return f.get().ok(); });
  };

  auto ctx = std::make_shared<grpc::ClientContext>();
  ApplyQueryParameters(*ctx, options, request);
  AddIdempotencyToken(*ctx, context);
  ApplyRoutingHeaders(*ctx, request);
  auto stream = stub_->WriteObject(std::move(ctx), options);

  auto splitter = SplitObjectWriteData<ContentType>(request.payload());
  std::int64_t offset = 0;

  // This loop must run at least once because we need to send at least one
  // Write() call for empty objects.
  do {
    proto_request.set_write_offset(offset);
    auto& data = *proto_request.mutable_checksummed_data();
    SetMutableContent(data, splitter.Next());
    data.set_crc32c(Crc32c(GetContent(data)));
    request.hash_function().Update(offset, GetContent(data), data.crc32c());
    offset += GetContent(data).size();

    auto wopts = grpc::WriteOptions{};
    MaybeFinalize(proto_request, wopts, request, !splitter.Done());

    auto watchdog = create_watchdog().then([&stream](auto f) {
      if (!f.get()) return false;
      stream->Cancel();
      return true;
    });
    auto success = stream->Write(proto_request, wopts);
    watchdog.cancel();
    if (watchdog.get()) {
      // The stream is cancelled by the watchdog. We still need to close it.
      (void)stream->Close();
      stream.reset();
      return TimeoutError(timeout, "Write()");
    }
    if (!success) {
      return HandleInsertObjectMediaError(timeout, create_watchdog,
                                          std::move(stream), options);
    }
    // After the first message, clear the object specification and checksums,
    // there is no need to resend it.
    proto_request.clear_write_object_spec();
    proto_request.clear_upload_id();
    proto_request.clear_object_checksums();
  } while (!splitter.Done());
  auto response = CloseWriteObjectStream(timeout, create_watchdog,
                                         std::move(stream), options);
  if (!response) return std::move(response).status();
  if (response->payload) return std::move(*response->payload);
  return storage::ObjectMetadata{};
}

StatusOr<storage::ObjectMetadata> GrpcStub::CopyObject(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::CopyObjectRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->RewriteObject(ctx, options, *proto);
  if (!response) return std::move(response).status();
  if (!response->done()) {
    return Status(
        StatusCode::kOutOfRange,
        "Object too large, use RewriteObject() instead of CopyObject()");
  }
  return FromProto(response->resource(), options);
}

StatusOr<storage::ObjectMetadata> GrpcStub::GetObjectMetadata(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetObjectMetadataRequest const& request) {
  auto response = GetObjectMetadataImpl(context, options, request);
  if (!response) return std::move(response).status();
  return FromProto(*response, options);
}

StatusOr<std::unique_ptr<storage::internal::ObjectReadSource>>
GrpcStub::ReadObject(rest_internal::RestContext& context,
                     Options const& options,
                     storage::internal::ReadObjectRangeRequest const& request) {
  auto ctx = std::make_shared<grpc::ClientContext>();
  ApplyQueryParameters(*ctx, options, request);
  AddIdempotencyToken(*ctx, context);
  auto proto_request = ToProto(request);
  if (!proto_request) return std::move(proto_request).status();
  auto stream = stub_->ReadObject(std::move(ctx), options, *proto_request);

  // The default timer source is a no-op. It does not set a timer, and always
  // returns an indication that the timer was cancelled.  `GrpcObjectReadSource`
  // takes no action on cancelled timers.
  GrpcObjectReadSource::TimerSource timer_source = [] {
    return make_ready_future(false);
  };
  auto const timeout = options.get<storage::DownloadStallTimeoutOption>();
  if (timeout != std::chrono::seconds(0)) {
    // Change to an active timer.
    timer_source = [timeout, cq = background_->cq()]() mutable {
      return cq.MakeRelativeTimer(timeout).then(
          [](auto f) { return f.get().ok(); });
    };
  }

  return std::unique_ptr<storage::internal::ObjectReadSource>(
      std::make_unique<GrpcObjectReadSource>(std::move(timer_source),
                                             std::move(stream)));
}

StatusOr<storage::internal::ListObjectsResponse> GrpcStub::ListObjects(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::ListObjectsRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->ListObjects(ctx, options, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response, options);
}

StatusOr<storage::internal::EmptyResponse> GrpcStub::DeleteObject(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::DeleteObjectRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->DeleteObject(ctx, options, proto);
  if (!response.ok()) return response;
  return storage::internal::EmptyResponse{};
}

StatusOr<storage::ObjectMetadata> GrpcStub::UpdateObject(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::UpdateObjectRequest const& request) {
  auto proto = ToProto(request);
  if (!proto) return std::move(proto).status();
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->UpdateObject(ctx, options, *proto);
  if (!response) return std::move(response).status();
  return FromProto(*response, options);
}

StatusOr<storage::ObjectMetadata> GrpcStub::PatchObject(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::PatchObjectRequest const& request) {
  auto response = PatchObjectImpl(context, options, request);
  if (!response) return std::move(response).status();
  return FromProto(*response, options);
}

StatusOr<storage::ObjectMetadata> GrpcStub::ComposeObject(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::ComposeObjectRequest const& request) {
  auto proto = ToProto(request);
  if (!proto) return std::move(proto).status();
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->ComposeObject(ctx, options, *proto);
  if (!response) return std::move(response).status();
  return FromProto(*response, options);
}

StatusOr<storage::internal::RewriteObjectResponse> GrpcStub::RewriteObject(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::RewriteObjectRequest const& request) {
  auto proto = ToProto(request);
  if (!proto) return std::move(proto).status();
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->RewriteObject(ctx, options, *proto);
  if (!response) return std::move(response).status();
  return FromProto(*response, options);
}

StatusOr<storage::internal::CreateResumableUploadResponse>
GrpcStub::CreateResumableUpload(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::ResumableUploadRequest const& request) {
  auto proto_request = ToProto(request);
  if (!proto_request) return std::move(proto_request).status();

  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto const timeout = options.get<storage::TransferStallTimeoutOption>();
  if (timeout.count() != 0) {
    ctx.set_deadline(std::chrono::system_clock::now() + timeout);
  }
  auto response = stub_->StartResumableWrite(ctx, options, *proto_request);
  if (!response.ok()) return std::move(response).status();

  return storage::internal::CreateResumableUploadResponse{
      response->upload_id()};
}

StatusOr<storage::internal::QueryResumableUploadResponse>
GrpcStub::QueryResumableUpload(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::QueryResumableUploadRequest const& request) {
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto const timeout = options.get<storage::TransferStallTimeoutOption>();
  if (timeout.count() != 0) {
    ctx.set_deadline(std::chrono::system_clock::now() + timeout);
  }
  auto response = stub_->QueryWriteStatus(ctx, options, ToProto(request));
  if (!response) return std::move(response).status();
  return FromProto(*response, options);
}

StatusOr<storage::internal::EmptyResponse> GrpcStub::DeleteResumableUpload(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::DeleteResumableUploadRequest const& request) {
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto const timeout = options.get<storage::TransferStallTimeoutOption>();
  if (timeout.count() != 0) {
    ctx.set_deadline(std::chrono::system_clock::now() + timeout);
  }
  auto response = stub_->CancelResumableWrite(ctx, options, ToProto(request));
  if (!response) return std::move(response).status();
  return storage::internal::EmptyResponse{};
}

StatusOr<storage::internal::QueryResumableUploadResponse> GrpcStub::UploadChunk(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::UploadChunkRequest const& request) {
  auto proto_request = google::storage::v2::WriteObjectRequest{};
  proto_request.set_upload_id(request.upload_session_url());

  auto const timeout = ScaleStallTimeout(
      options.get<storage::TransferStallTimeoutOption>(),
      options.get<storage::TransferStallMinimumRateOption>(),
      google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES);

  auto create_watchdog = [cq = background_->cq(), timeout]() mutable {
    if (timeout == std::chrono::seconds(0)) {
      return make_ready_future(false);
    }
    return cq.MakeRelativeTimer(timeout).then(
        [](auto f) { return f.get().ok(); });
  };

  auto ctx = std::make_shared<grpc::ClientContext>();
  ApplyQueryParameters(*ctx, options, request);
  AddIdempotencyToken(*ctx, context);
  ApplyRoutingHeaders(*ctx, request);
  auto stream = stub_->WriteObject(std::move(ctx), options);

  auto splitter = SplitObjectWriteData<ContentType>(request.payload());
  auto offset = request.offset();

  // This loop must run at least once because we need to send at least one
  // Write() call for empty objects.
  do {
    proto_request.set_write_offset(offset);
    auto& data = *proto_request.mutable_checksummed_data();
    SetMutableContent(data, splitter.Next());
    data.set_crc32c(Crc32c(GetContent(data)));
    request.hash_function().Update(offset, GetContent(data), data.crc32c());
    offset += GetContent(data).size();

    auto wopts = grpc::WriteOptions();
    MaybeFinalize(proto_request, wopts, request, !splitter.Done());

    auto watchdog = create_watchdog().then([&stream](auto f) {
      if (!f.get()) return false;
      stream->Cancel();
      return true;
    });
    auto success = stream->Write(proto_request, wopts);
    watchdog.cancel();
    if (watchdog.get()) {
      // The stream is cancelled by the watchdog. We still need to close it.
      (void)stream->Close();
      stream.reset();
      return TimeoutError(timeout, "Write()");
    }
    if (!success) {
      return HandleUploadChunkError(timeout, create_watchdog, std::move(stream),
                                    options);
    }
    // After the first message, clear the object specification and checksums,
    // there is no need to resend it.
    proto_request.clear_write_object_spec();
    proto_request.clear_upload_id();
    proto_request.clear_object_checksums();
  } while (!splitter.Done());
  return CloseWriteObjectStream(timeout, create_watchdog, std::move(stream),
                                options);
}

StatusOr<storage::internal::ListBucketAclResponse> GrpcStub::ListBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::ListBucketAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto get = GetBucketMetadata(context, options, get_request);
  if (!get) return std::move(get).status();
  storage::internal::ListBucketAclResponse response;
  response.items = get->acl();
  return response;
}

StatusOr<storage::BucketAccessControl> GrpcStub::GetBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetBucketAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto get = GetBucketMetadataImpl(context, options, get_request);
  auto const bucket_self_link =
      SyntheticSelfLinkBucket(options, request.bucket_name());
  return FindBucketAccessControl(std::move(get), request.entity(),
                                 bucket_self_link);
}

StatusOr<storage::BucketAccessControl> GrpcStub::CreateBucketAcl(
    rest_internal::RestContext&, Options const& options,
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
      SyntheticSelfLinkBucket(options, request.bucket_name());
  return FindBucketAccessControl(
      ModifyBucketAccessControl(options, get_request, updater),
      request.entity(), bucket_self_link);
}

StatusOr<storage::internal::EmptyResponse> GrpcStub::DeleteBucketAcl(
    rest_internal::RestContext&, Options const& options,
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
  auto response = ModifyBucketAccessControl(options, get_request, updater);
  if (!response) return std::move(response).status();
  return storage::internal::EmptyResponse{};
}

StatusOr<storage::BucketAccessControl> GrpcStub::UpdateBucketAcl(
    rest_internal::RestContext&, Options const& options,
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
      SyntheticSelfLinkBucket(options, request.bucket_name());
  return FindBucketAccessControl(
      ModifyBucketAccessControl(options, get_request, updater),
      request.entity(), bucket_self_link);
}

StatusOr<storage::BucketAccessControl> GrpcStub::PatchBucketAcl(
    rest_internal::RestContext&, Options const& options,
    storage::internal::PatchBucketAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_multiple_options(storage::Projection("full"));
  auto updater = [&request](BucketAccessControlList acl) {
    return UpsertAcl(std::move(acl), request.entity(), Role(request.patch()));
  };
  auto const bucket_self_link =
      SyntheticSelfLinkBucket(options, request.bucket_name());
  return FindBucketAccessControl(
      ModifyBucketAccessControl(options, get_request, updater),
      request.entity(), bucket_self_link);
}

StatusOr<storage::internal::ListObjectAclResponse> GrpcStub::ListObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::ListObjectAclRequest const& request) {
  auto get_request = storage::internal::GetObjectMetadataRequest(
      request.bucket_name(), request.object_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto get = GetObjectMetadata(context, options, get_request);
  if (!get) return std::move(get).status();
  storage::internal::ListObjectAclResponse response;
  response.items = get->acl();
  return response;
}

StatusOr<storage::ObjectAccessControl> GrpcStub::CreateObjectAcl(
    rest_internal::RestContext&, Options const& options,
    storage::internal::CreateObjectAclRequest const& request) {
  auto get_request = storage::internal::GetObjectMetadataRequest(
      request.bucket_name(), request.object_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto updater = [&request](ObjectAccessControlList acl) {
    return UpsertAcl(std::move(acl), request.entity(), request.role());
  };
  auto const object_self_link = SyntheticSelfLinkObject(
      options, request.bucket_name(), request.object_name());
  return FindObjectAccessControl(
      ModifyObjectAccessControl(options, get_request, updater),
      request.entity(), object_self_link);
}

StatusOr<storage::internal::EmptyResponse> GrpcStub::DeleteObjectAcl(
    rest_internal::RestContext&, Options const& options,
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
  auto response = ModifyObjectAccessControl(options, get_request, updater);
  if (!response) return std::move(response).status();
  return storage::internal::EmptyResponse{};
}

StatusOr<storage::ObjectAccessControl> GrpcStub::GetObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetObjectAclRequest const& request) {
  auto get_request = storage::internal::GetObjectMetadataRequest(
      request.bucket_name(), request.object_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto get = GetObjectMetadataImpl(context, options, get_request);
  auto const object_self_link = SyntheticSelfLinkObject(
      options, request.bucket_name(), request.object_name());
  return FindObjectAccessControl(std::move(get), request.entity(),
                                 object_self_link);
}

StatusOr<storage::ObjectAccessControl> GrpcStub::UpdateObjectAcl(
    rest_internal::RestContext&, Options const& options,
    storage::internal::UpdateObjectAclRequest const& request) {
  auto get_request = storage::internal::GetObjectMetadataRequest(
      request.bucket_name(), request.object_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto updater = [&request](ObjectAccessControlList acl) {
    return UpsertAcl(std::move(acl), request.entity(), request.role());
  };
  auto const object_self_link = SyntheticSelfLinkObject(
      options, request.bucket_name(), request.object_name());
  return FindObjectAccessControl(
      ModifyObjectAccessControl(options, get_request, updater),
      request.entity(), object_self_link);
}

StatusOr<storage::ObjectAccessControl> GrpcStub::PatchObjectAcl(
    rest_internal::RestContext&, Options const& options,
    storage::internal::PatchObjectAclRequest const& request) {
  auto get_request = storage::internal::GetObjectMetadataRequest(
      request.bucket_name(), request.object_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto updater = [&request](ObjectAccessControlList acl) {
    return UpsertAcl(std::move(acl), request.entity(), Role(request.patch()));
  };
  auto const object_self_link = SyntheticSelfLinkObject(
      options, request.bucket_name(), request.object_name());
  return FindObjectAccessControl(
      ModifyObjectAccessControl(options, get_request, updater),
      request.entity(), object_self_link);
}

StatusOr<storage::internal::ListDefaultObjectAclResponse>
GrpcStub::ListDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::ListDefaultObjectAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto get = GetBucketMetadata(context, options, get_request);
  if (!get) return std::move(get).status();
  storage::internal::ListDefaultObjectAclResponse response;
  response.items = get->default_acl();
  return response;
}

StatusOr<storage::ObjectAccessControl> GrpcStub::CreateDefaultObjectAcl(
    rest_internal::RestContext&, Options const& options,
    storage::internal::CreateDefaultObjectAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto updater = [&request](ObjectAccessControlList acl) {
    return UpsertAcl(std::move(acl), request.entity(), request.role());
  };
  return FindDefaultObjectAccessControl(
      ModifyDefaultAccessControl(options, get_request, updater),
      request.entity());
}

StatusOr<storage::internal::EmptyResponse> GrpcStub::DeleteDefaultObjectAcl(
    rest_internal::RestContext&, Options const& options,
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
  auto response = ModifyDefaultAccessControl(options, get_request, updater);
  if (!response) return std::move(response).status();
  return storage::internal::EmptyResponse{};
}

StatusOr<storage::ObjectAccessControl> GrpcStub::GetDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetDefaultObjectAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto get = GetBucketMetadataImpl(context, options, get_request);
  return FindDefaultObjectAccessControl(std::move(get), request.entity());
}

StatusOr<storage::ObjectAccessControl> GrpcStub::UpdateDefaultObjectAcl(
    rest_internal::RestContext&, Options const& options,
    storage::internal::UpdateDefaultObjectAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto updater = [&request](ObjectAccessControlList acl) {
    return UpsertAcl(std::move(acl), request.entity(), request.role());
  };
  return FindDefaultObjectAccessControl(
      ModifyDefaultAccessControl(options, get_request, updater),
      request.entity());
}

StatusOr<storage::ObjectAccessControl> GrpcStub::PatchDefaultObjectAcl(
    rest_internal::RestContext&, Options const& options,
    storage::internal::PatchDefaultObjectAclRequest const& request) {
  auto get_request =
      storage::internal::GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  get_request.set_option(storage::Projection("full"));
  auto updater = [&request](ObjectAccessControlList acl) {
    return UpsertAcl(std::move(acl), request.entity(), Role(request.patch()));
  };
  return FindDefaultObjectAccessControl(
      ModifyDefaultAccessControl(options, get_request, updater),
      request.entity());
}

StatusOr<storage::ServiceAccount> GrpcStub::GetServiceAccount(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetProjectServiceAccountRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->GetServiceAccount(ctx, options, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::internal::ListHmacKeysResponse> GrpcStub::ListHmacKeys(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::ListHmacKeysRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->ListHmacKeys(ctx, options, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::internal::CreateHmacKeyResponse> GrpcStub::CreateHmacKey(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::CreateHmacKeyRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->CreateHmacKey(ctx, options, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::internal::EmptyResponse> GrpcStub::DeleteHmacKey(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::DeleteHmacKeyRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->DeleteHmacKey(ctx, options, proto);
  if (!response.ok()) return response;
  return storage::internal::EmptyResponse{};
}

StatusOr<storage::HmacKeyMetadata> GrpcStub::GetHmacKey(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetHmacKeyRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->GetHmacKey(ctx, options, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::HmacKeyMetadata> GrpcStub::UpdateHmacKey(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::UpdateHmacKeyRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->UpdateHmacKey(ctx, options, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::internal::SignBlobResponse> GrpcStub::SignBlob(
    rest_internal::RestContext& context, Options const&,
    storage::internal::SignBlobRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  // This request does not have any options that require using
  //     ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = iam_stub_->SignBlob(ctx, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::internal::ListNotificationsResponse>
GrpcStub::ListNotifications(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::ListNotificationsRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->ListNotificationConfigs(ctx, options, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::NotificationMetadata> GrpcStub::CreateNotification(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::CreateNotificationRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->CreateNotificationConfig(ctx, options, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::NotificationMetadata> GrpcStub::GetNotification(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetNotificationRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->GetNotificationConfig(ctx, options, proto);
  if (!response) return std::move(response).status();
  return FromProto(*response);
}

StatusOr<storage::internal::EmptyResponse> GrpcStub::DeleteNotification(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::DeleteNotificationRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  auto response = stub_->DeleteNotificationConfig(ctx, options, proto);
  if (!response.ok()) return response;
  return storage::internal::EmptyResponse{};
}

std::vector<std::string> GrpcStub::InspectStackStructure() const {
  return {"GrpcStub"};
}

StatusOr<google::storage::v2::Bucket> GrpcStub::GetBucketMetadataImpl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetBucketMetadataRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  return stub_->GetBucket(ctx, options, proto);
}

StatusOr<google::storage::v2::Bucket> GrpcStub::PatchBucketImpl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::PatchBucketRequest const& request) {
  auto proto = ToProto(request);
  if (!proto) return std::move(proto).status();
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  return stub_->UpdateBucket(ctx, options, *proto);
}

StatusOr<google::storage::v2::Object> GrpcStub::GetObjectMetadataImpl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetObjectMetadataRequest const& request) {
  auto proto = ToProto(request);
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  return stub_->GetObject(ctx, options, proto);
}

StatusOr<google::storage::v2::Object> GrpcStub::PatchObjectImpl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::PatchObjectRequest const& request) {
  auto proto = ToProto(request);
  if (!proto) return std::move(proto).status();
  grpc::ClientContext ctx;
  ApplyQueryParameters(ctx, options, request);
  AddIdempotencyToken(ctx, context);
  return stub_->UpdateObject(ctx, options, *proto);
}

StatusOr<google::storage::v2::Bucket> GrpcStub::ModifyBucketAccessControl(
    Options const& options,
    storage::internal::GetBucketMetadataRequest const& request,
    BucketAclUpdater const& updater) {
  auto context = rest_internal::RestContext{};
  auto response = GetBucketMetadataImpl(context, options, request);
  if (!response) return std::move(response).status();
  auto acl = updater(std::move(*response->mutable_acl()));
  if (!acl) return std::move(acl).status();

  auto const bucket_self_link =
      SyntheticSelfLinkBucket(options, request.bucket_name());
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
  auto patch = PatchBucketImpl(context, options, patch_request);
  // Retry on failed preconditions
  if (patch.status().code() == StatusCode::kFailedPrecondition) {
    return Status(
        StatusCode::kUnavailable,
        "retrying BucketAccessControl change due to conflict, bucket=" +
            request.bucket_name());
  }
  return patch;
}

StatusOr<google::storage::v2::Object> GrpcStub::ModifyObjectAccessControl(
    Options const& options,
    storage::internal::GetObjectMetadataRequest const& request,
    ObjectAclUpdater const& updater) {
  auto context = rest_internal::RestContext{};
  auto response = GetObjectMetadataImpl(context, options, request);
  if (!response) return std::move(response).status();
  auto acl = updater(std::move(*response->mutable_acl()));
  if (!acl) return std::move(acl).status();

  std::vector<storage::ObjectAccessControl> updated(acl->size());
  auto const object_self_link = SyntheticSelfLinkObject(
      options, request.bucket_name(), request.object_name());
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
  auto patch = PatchObjectImpl(context, options, patch_request);
  // Retry on failed preconditions
  if (patch.status().code() == StatusCode::kFailedPrecondition) {
    return Status(
        StatusCode::kUnavailable,
        "retrying ObjectAccessControl change due to conflict, bucket=" +
            request.bucket_name() + ", object=" + request.object_name());
  }
  return patch;
}

StatusOr<google::storage::v2::Bucket> GrpcStub::ModifyDefaultAccessControl(
    Options const& options,
    storage::internal::GetBucketMetadataRequest const& request,
    DefaultObjectAclUpdater const& updater) {
  auto context = rest_internal::RestContext{};
  auto response = GetBucketMetadataImpl(context, options, request);
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
  auto patch = PatchBucketImpl(context, options, patch_request);
  // Retry on failed preconditions
  if (patch.status().code() == StatusCode::kFailedPrecondition) {
    return Status(
        StatusCode::kUnavailable,
        "retrying BucketAccessControl change due to conflict, bucket=" +
            request.bucket_name());
  }
  return patch;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
