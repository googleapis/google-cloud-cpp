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
#include "google/cloud/storage/internal/grpc_bucket_access_control_parser.h"
#include "google/cloud/storage/internal/grpc_bucket_metadata_parser.h"
#include "google/cloud/storage/internal/grpc_bucket_request_parser.h"
#include "google/cloud/storage/internal/grpc_configure_client_context.h"
#include "google/cloud/storage/internal/grpc_hmac_key_metadata_parser.h"
#include "google/cloud/storage/internal/grpc_hmac_key_request_parser.h"
#include "google/cloud/storage/internal/grpc_object_access_control_parser.h"
#include "google/cloud/storage/internal/grpc_object_metadata_parser.h"
#include "google/cloud/storage/internal/grpc_object_read_source.h"
#include "google/cloud/storage/internal/grpc_object_request_parser.h"
#include "google/cloud/storage/internal/grpc_service_account_parser.h"
#include "google/cloud/storage/internal/storage_stub_factory.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/big_endian.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/log.h"
#include "absl/strings/match.h"
#include <crc32c/crc32c.h>
#include <grpcpp/grpcpp.h>
#include <algorithm>
#include <cinttypes>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

template <typename AccessControl>
StatusOr<std::vector<AccessControl>> UpsertAcl(std::vector<AccessControl> acl,
                                               std::string const& entity,
                                               std::string const& role) {
  auto i = std::find_if(
      acl.begin(), acl.end(),
      [&](AccessControl const& entry) { return entry.entity() == entity; });
  if (i != acl.end()) {
    i->set_role(role);
    return acl;
  }
  acl.push_back(AccessControl().set_entity(entity).set_role(role));
  return acl;
}

// Used in the implementation of `*BucketAcl()`.
StatusOr<BucketAccessControl> FindBucketAccessControl(
    StatusOr<BucketMetadata> response, std::string const& entity) {
  if (!response) return std::move(response).status();
  for (auto const& acl : response->acl()) {
    if (acl.entity() == entity) return acl;
  }
  return Status(StatusCode::kNotFound, "cannot find entity <" + entity +
                                           "> in bucket " + response->name());
}

// Used in the implementation of `*ObjectAcl()`.
StatusOr<ObjectAccessControl> FindObjectAccessControl(
    StatusOr<ObjectMetadata> response, std::string const& entity) {
  if (!response) return std::move(response).status();
  for (auto const& acl : response->acl()) {
    if (acl.entity() == entity) return acl;
  }
  return Status(StatusCode::kNotFound, "cannot find entity <" + entity +
                                           "> in object id " + response->id());
}

// Used in the implementation of `*DefaultObjectAcl()`.
StatusOr<ObjectAccessControl> FindDefaultObjectAccessControl(
    StatusOr<BucketMetadata> response, std::string const& entity) {
  if (!response) return std::move(response).status();
  for (auto const& acl : response->default_acl()) {
    if (acl.entity() == entity) return acl;
  }
  return Status(StatusCode::kNotFound, "cannot find entity <" + entity +
                                           "> in object id " + response->id());
}

std::chrono::milliseconds DefaultTransferStallTimeout(
    std::chrono::milliseconds value) {
  if (value != std::chrono::milliseconds(0)) return value;
  // We need a large value for `wait_for()`, but not so large that it can easily
  // overflow.  Fortunately, uploads automatically cancel (server side) after
  // 7 days, so waiting for 14 days will not create spurious timeouts.
  return std::chrono::milliseconds(std::chrono::hours(24) * 14);
}

// If this is the last `Write()` call of the last `UploadChunk()` set the flags
// to finalize the request
void MaybeFinalize(google::storage::v2::WriteObjectRequest& write_request,
                   grpc::WriteOptions& options,
                   UploadChunkRequest const& request, bool has_more) {
  if (!request.last_chunk() || has_more) return;
  write_request.set_finish_write(true);
  options.set_last_message();
  auto const& hashes = request.full_object_hashes();
  if (!hashes.md5.empty()) {
    auto md5 = GrpcObjectMetadataParser::MD5ToProto(hashes.md5);
    if (md5) {
      write_request.mutable_object_checksums()->set_md5_hash(*std::move(md5));
    }
  }
  if (!hashes.crc32c.empty()) {
    auto crc32c = GrpcObjectMetadataParser::Crc32cToProto(hashes.crc32c);
    if (crc32c) {
      write_request.mutable_object_checksums()->set_crc32c(*std::move(crc32c));
    }
  }
}

Status TimeoutError(std::chrono::milliseconds timeout, std::string const& op) {
  return Status(StatusCode::kDeadlineExceeded,
                "timeout [" + absl::FormatDuration(absl::FromChrono(timeout)) +
                    "] while waiting for " + op);
}

struct WaitForFinish {
  std::unique_ptr<GrpcClient::WriteObjectStream> stream;
  void operator()(future<StatusOr<google::storage::v2::WriteObjectResponse>>) {}
};

struct WaitForIdle {
  std::unique_ptr<GrpcClient::WriteObjectStream> stream;
  void operator()(future<bool>) {
    auto finish = stream->Finish();
    (void)finish.then(WaitForFinish{std::move(stream)});
  }
};

StatusOr<QueryResumableUploadResponse> CloseWriteObjectStream(
    std::chrono::milliseconds timeout,
    std::unique_ptr<GrpcClient::WriteObjectStream> writer,
    bool sent_last_message, google::cloud::Options const& options) {
  if (!writer) return TimeoutError(timeout, "Write()");
  if (!sent_last_message) {
    auto pending = writer->WritesDone();
    if (pending.wait_for(timeout) == std::future_status::timeout) {
      writer->Cancel();
      pending.then(WaitForIdle{std::move(writer)});
      return TimeoutError(timeout, "WritesDone()");
    }
  }
  auto pending = writer->Finish();
  if (pending.wait_for(timeout) == std::future_status::timeout) {
    writer->Cancel();
    pending.then(WaitForFinish{std::move(writer)});
    return TimeoutError(timeout, "Finish()");
  }
  auto response = pending.get();
  if (!response) return std::move(response).status();
  return GrpcObjectRequestParser::FromProto(*std::move(response), options);
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

  options = DefaultOptionsWithCredentials(std::move(options));
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
    std::shared_ptr<storage_internal::StorageStub> stub, Options opts) {
  return std::shared_ptr<GrpcClient>(
      new GrpcClient(std::move(stub), DefaultOptionsGrpc(std::move(opts))));
}

GrpcClient::GrpcClient(Options opts)
    : options_(std::move(opts)),
      backwards_compatibility_options_(
          MakeBackwardsCompatibleClientOptions(options_)),
      background_(MakeBackgroundThreadsFactory(options_)()),
      stub_(storage_internal::CreateStorageStub(background_->cq(), options_)) {}

GrpcClient::GrpcClient(std::shared_ptr<storage_internal::StorageStub> stub,
                       Options opts)
    : options_(std::move(opts)),
      backwards_compatibility_options_(
          MakeBackwardsCompatibleClientOptions(options_)),
      background_(MakeBackgroundThreadsFactory(options_)()),
      stub_(std::move(stub)) {}

ClientOptions const& GrpcClient::client_options() const {
  return backwards_compatibility_options_;
}

Options GrpcClient::options() const { return options_; }

StatusOr<ListBucketsResponse> GrpcClient::ListBuckets(
    ListBucketsRequest const& request) {
  auto proto = GrpcBucketRequestParser::ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->ListBuckets(context, proto);
  if (!response) return std::move(response).status();
  return GrpcBucketRequestParser::FromProto(*response);
}

StatusOr<BucketMetadata> GrpcClient::CreateBucket(
    CreateBucketRequest const& request) {
  auto proto = GrpcBucketRequestParser::ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->CreateBucket(context, proto);
  if (!response) return std::move(response).status();
  return GrpcBucketMetadataParser::FromProto(*response);
}

StatusOr<BucketMetadata> GrpcClient::GetBucketMetadata(
    GetBucketMetadataRequest const& request) {
  auto proto = GrpcBucketRequestParser::ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->GetBucket(context, proto);
  if (!response) return std::move(response).status();
  return GrpcBucketMetadataParser::FromProto(*response);
}

StatusOr<EmptyResponse> GrpcClient::DeleteBucket(
    DeleteBucketRequest const& request) {
  auto proto = GrpcBucketRequestParser::ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto status = stub_->DeleteBucket(context, proto);
  if (!status.ok()) return status;
  return EmptyResponse{};
}

StatusOr<BucketMetadata> GrpcClient::UpdateBucket(
    UpdateBucketRequest const& request) {
  auto proto = GrpcBucketRequestParser::ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->UpdateBucket(context, proto);
  if (!response) return std::move(response).status();
  return GrpcBucketMetadataParser::FromProto(*response);
}

StatusOr<BucketMetadata> GrpcClient::PatchBucket(
    PatchBucketRequest const& request) {
  auto proto = GrpcBucketRequestParser::ToProto(request);
  if (!proto) return std::move(proto).status();
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->UpdateBucket(context, *proto);
  if (!response) return std::move(response).status();
  return GrpcBucketMetadataParser::FromProto(*response);
}

StatusOr<NativeIamPolicy> GrpcClient::GetNativeBucketIamPolicy(
    GetBucketIamPolicyRequest const& request) {
  auto proto = GrpcBucketRequestParser::ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->GetIamPolicy(context, proto);
  if (!response) return std::move(response).status();
  return GrpcBucketRequestParser::FromProto(*response);
}

StatusOr<NativeIamPolicy> GrpcClient::SetNativeBucketIamPolicy(
    SetNativeBucketIamPolicyRequest const& request) {
  auto proto = GrpcBucketRequestParser::ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->SetIamPolicy(context, proto);
  if (!response) return std::move(response).status();
  return GrpcBucketRequestParser::FromProto(*response);
}

StatusOr<TestBucketIamPermissionsResponse> GrpcClient::TestBucketIamPermissions(
    TestBucketIamPermissionsRequest const& request) {
  auto proto = GrpcBucketRequestParser::ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->TestIamPermissions(context, proto);
  if (!response) return std::move(response).status();
  return GrpcBucketRequestParser::FromProto(*response);
}

StatusOr<BucketMetadata> GrpcClient::LockBucketRetentionPolicy(
    LockBucketRetentionPolicyRequest const& request) {
  auto proto = GrpcBucketRequestParser::ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->LockBucketRetentionPolicy(context, proto);
  if (!response) return std::move(response).status();
  return GrpcBucketMetadataParser::FromProto(*response);
}

StatusOr<ObjectMetadata> GrpcClient::InsertObjectMedia(
    InsertObjectMediaRequest const& request) {
  auto r = GrpcObjectRequestParser::ToProto(request);
  if (!r) return std::move(r).status();
  auto proto_request = *r;

  struct WaitForFinish {
    std::unique_ptr<WriteObjectStream> stream;
    void operator()(
        future<StatusOr<google::storage::v2::WriteObjectResponse>>) {}
  };
  struct WaitForIdle {
    std::unique_ptr<WriteObjectStream> stream;
    void operator()(future<bool>) {
      auto finish = stream->Finish();
      (void)finish.then(WaitForFinish{std::move(stream)});
    }
  };

  auto const timeout =
      DefaultTransferStallTimeout(google::cloud::internal::CurrentOptions()
                                      .get<TransferStallTimeoutOption>());

  auto context = absl::make_unique<grpc::ClientContext>();
  // The REST response is just the object metadata (aka the "resource"). In the
  // gRPC response the object metadata is in a "resource" field. Passing an
  // extra prefix to ApplyQueryParameters sends the right filtering instructions
  // to the gRPC API.
  ApplyQueryParameters(*context, request, "resource");
  auto stream = stub_->AsyncWriteObject(background_->cq(), std::move(context));

  auto pending_start = stream->Start();
  if (pending_start.wait_for(timeout) == std::future_status::timeout) {
    stream->Cancel();
    pending_start.then(WaitForIdle{std::move(stream)});
    return Status(StatusCode::kDeadlineExceeded,
                  "timeout [" +
                      absl::FormatDuration(absl::FromChrono(timeout)) +
                      "] while waiting for Start()");
  }

  auto const& contents = request.contents();
  auto const contents_size = static_cast<std::int64_t>(contents.size());
  std::int64_t const maximum_buffer_size =
      google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES;

  // This loop must run at least once because we need to send at least one
  // Write() call for empty objects.
  std::int64_t n;
  for (std::int64_t offset = 0; offset <= contents_size; offset += n) {
    proto_request.set_write_offset(offset);
    auto& data = *proto_request.mutable_checksummed_data();
    n = (std::min)(contents_size - offset, maximum_buffer_size);
    data.set_content(
        contents.substr(static_cast<std::string::size_type>(offset),
                        static_cast<std::string::size_type>(n)));
    data.set_crc32c(crc32c::Crc32c(data.content()));

    auto write_options = grpc::WriteOptions{};
    if (offset + n >= contents_size) {
      proto_request.set_finish_write(true);
      write_options.set_last_message();
    }
    auto pending = stream->Write(proto_request, write_options);
    if (pending.wait_for(timeout) == std::future_status::timeout) {
      stream->Cancel();
      pending.then(WaitForIdle{std::move(stream)});
      return Status(StatusCode::kDeadlineExceeded,
                    "timeout [" +
                        absl::FormatDuration(absl::FromChrono(timeout)) +
                        "] while waiting for Write()");
    }

    if (!pending.get() || proto_request.finish_write()) break;
    // After the first message, clear the object specification and checksums,
    // there is no need to resend it.
    proto_request.clear_write_object_spec();
    proto_request.clear_object_checksums();
  }

  auto pending = stream->Finish();
  if (pending.wait_for(timeout) == std::future_status::timeout) {
    stream->Cancel();
    pending.then(WaitForFinish{std::move(stream)});
    return Status(StatusCode::kDeadlineExceeded,
                  "timeout [" +
                      absl::FormatDuration(absl::FromChrono(timeout)) +
                      "] while waiting for Finish()");
  }

  auto response = pending.get();
  if (!response) return std::move(response).status();
  if (response->has_resource()) {
    return GrpcObjectMetadataParser::FromProto(response->resource(), options());
  }
  return ObjectMetadata{};
}

StatusOr<ObjectMetadata> GrpcClient::CopyObject(
    CopyObjectRequest const& request) {
  auto proto = GrpcObjectRequestParser::ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request, "resource");
  auto response = stub_->RewriteObject(context, *proto);
  if (!response) return std::move(response).status();
  if (!response->done()) {
    return Status(
        StatusCode::kOutOfRange,
        "Object too large, use RewriteObject() instead of CopyObject()");
  }
  return GrpcObjectMetadataParser::FromProto(response->resource(),
                                             CurrentOptions());
}

StatusOr<ObjectMetadata> GrpcClient::GetObjectMetadata(
    GetObjectMetadataRequest const& request) {
  auto proto = GrpcObjectRequestParser::ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->GetObject(context, proto);
  if (!response) return std::move(response).status();
  return GrpcObjectMetadataParser::FromProto(*response, CurrentOptions());
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
  auto proto_request = GrpcObjectRequestParser::ToProto(request);
  if (!proto_request) return std::move(proto_request).status();
  auto stream = stub_->ReadObject(std::move(context), *proto_request);

  // The default timer source is a no-op. It does not set a timer, and always
  // returns an indication that the timer expired.  The GrpcObjectReadSource
  // takes no action on expired timers.
  GrpcObjectReadSource::TimerSource timer_source = [] {
    return make_ready_future(false);
  };
  auto const timeout = CurrentOptions().get<DownloadStallTimeoutOption>();
  if (timeout != std::chrono::seconds(0)) {
    // Change to an active timer.
    timer_source = [timeout, cq = background_->cq()]() mutable {
      return cq.MakeRelativeTimer(timeout).then(
          [](auto f) { return f.get().ok(); });
    };
  }

  return std::unique_ptr<ObjectReadSource>(
      absl::make_unique<GrpcObjectReadSource>(std::move(timer_source),
                                              std::move(stream)));
}

StatusOr<ListObjectsResponse> GrpcClient::ListObjects(
    ListObjectsRequest const& request) {
  auto proto = GrpcObjectRequestParser::ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->ListObjects(context, proto);
  if (!response) return std::move(response).status();
  return GrpcObjectRequestParser::FromProto(*response, CurrentOptions());
}

StatusOr<EmptyResponse> GrpcClient::DeleteObject(
    DeleteObjectRequest const& request) {
  auto proto = GrpcObjectRequestParser::ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->DeleteObject(context, proto);
  if (!response.ok()) return response;
  return EmptyResponse{};
}

StatusOr<ObjectMetadata> GrpcClient::UpdateObject(
    UpdateObjectRequest const& request) {
  auto proto = GrpcObjectRequestParser::ToProto(request);
  if (!proto) return std::move(proto).status();
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->UpdateObject(context, *proto);
  if (!response) return std::move(response).status();
  return GrpcObjectMetadataParser::FromProto(*response, CurrentOptions());
}

StatusOr<ObjectMetadata> GrpcClient::PatchObject(
    PatchObjectRequest const& request) {
  auto proto = GrpcObjectRequestParser::ToProto(request);
  if (!proto) return std::move(proto).status();
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->UpdateObject(context, *proto);
  if (!response) return std::move(response).status();
  return GrpcObjectMetadataParser::FromProto(*response, CurrentOptions());
}

StatusOr<ObjectMetadata> GrpcClient::ComposeObject(
    ComposeObjectRequest const& request) {
  auto proto = GrpcObjectRequestParser::ToProto(request);
  if (!proto) return std::move(proto).status();
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->ComposeObject(context, *proto);
  if (!response) return std::move(response).status();
  return GrpcObjectMetadataParser::FromProto(*response, CurrentOptions());
}

StatusOr<RewriteObjectResponse> GrpcClient::RewriteObject(
    RewriteObjectRequest const& request) {
  auto proto = GrpcObjectRequestParser::ToProto(request);
  if (!proto) return std::move(proto).status();
  grpc::ClientContext context;
  ApplyQueryParameters(context, request, "resource");
  auto response = stub_->RewriteObject(context, *proto);
  if (!response) return std::move(response).status();
  return GrpcObjectRequestParser::FromProto(*response, CurrentOptions());
}

StatusOr<CreateResumableUploadResponse> GrpcClient::CreateResumableUpload(
    ResumableUploadRequest const& request) {
  auto proto_request = GrpcObjectRequestParser::ToProto(request);
  if (!proto_request) return std::move(proto_request).status();

  grpc::ClientContext context;
  ApplyQueryParameters(context, request, "resource");
  auto const timeout = CurrentOptions().get<TransferStallTimeoutOption>();
  if (timeout.count() != 0) {
    context.set_deadline(std::chrono::system_clock::now() + timeout);
  }
  auto response = stub_->StartResumableWrite(context, *proto_request);
  if (!response.ok()) return std::move(response).status();

  return CreateResumableUploadResponse{response->upload_id()};
}

StatusOr<QueryResumableUploadResponse> GrpcClient::QueryResumableUpload(
    QueryResumableUploadRequest const& request) {
  grpc::ClientContext context;
  ApplyQueryParameters(context, request, "resource");
  auto const timeout = CurrentOptions().get<TransferStallTimeoutOption>();
  if (timeout.count() != 0) {
    context.set_deadline(std::chrono::system_clock::now() + timeout);
  }
  auto response = stub_->QueryWriteStatus(
      context, GrpcObjectRequestParser::ToProto(request));
  if (!response) return std::move(response).status();
  return GrpcObjectRequestParser::FromProto(*response, options());
}

StatusOr<EmptyResponse> GrpcClient::DeleteResumableUpload(
    DeleteResumableUploadRequest const& request) {
  grpc::ClientContext context;
  ApplyQueryParameters(context, request, "");
  auto const timeout = CurrentOptions().get<TransferStallTimeoutOption>();
  if (timeout.count() != 0) {
    context.set_deadline(std::chrono::system_clock::now() + timeout);
  }
  auto response = stub_->CancelResumableWrite(
      context, GrpcObjectRequestParser::ToProto(request));
  if (!response) return std::move(response).status();
  return EmptyResponse{};
}

StatusOr<QueryResumableUploadResponse> GrpcClient::UploadChunk(
    UploadChunkRequest const& request) {
  auto const timeout =
      DefaultTransferStallTimeout(google::cloud::internal::CurrentOptions()
                                      .get<TransferStallTimeoutOption>());

  auto context = absl::make_unique<grpc::ClientContext>();
  ApplyQueryParameters(*context, request, "resource");
  auto writer = stub_->AsyncWriteObject(background_->cq(), std::move(context));

  auto pending_start = writer->Start();
  if (pending_start.wait_for(timeout) == std::future_status::timeout) {
    writer->Cancel();
    pending_start.then(WaitForIdle{std::move(writer)});
    return TimeoutError(timeout, "Start()");
  }

  std::size_t const maximum_chunk_size =
      google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES;
  std::string chunk;
  chunk.reserve(maximum_chunk_size);
  auto offset = static_cast<google::protobuf::int64>(request.offset());

  bool sent_last_message = false;
  auto flush_chunk = [&](bool has_more) {
    if (chunk.size() < maximum_chunk_size && has_more) return true;
    if (chunk.empty() && !request.last_chunk()) return true;

    google::storage::v2::WriteObjectRequest write_request;
    write_request.set_upload_id(request.upload_session_url());
    write_request.set_write_offset(offset);
    write_request.set_finish_write(false);
    auto write_size = chunk.size();

    auto& data = *write_request.mutable_checksummed_data();
    data.set_content(std::move(chunk));
    chunk.clear();
    chunk.reserve(maximum_chunk_size);
    data.set_crc32c(crc32c::Crc32c(data.content()));

    auto options = grpc::WriteOptions();
    MaybeFinalize(write_request, options, request, has_more);

    auto pending = writer->Write(write_request, options);
    if (pending.wait_for(timeout) == std::future_status::timeout) {
      writer->Cancel();
      pending.then(WaitForIdle{std::move(writer)});
      return false;
    }

    sent_last_message = options.is_last_message();
    if (!pending.get()) return false;
    // After the first message, clear the object specification and checksums,
    // there is no need to resend it.
    write_request.clear_write_object_spec();
    write_request.clear_object_checksums();
    offset += write_size;

    return true;
  };

  auto close_writer = [&]() -> StatusOr<QueryResumableUploadResponse> {
    return CloseWriteObjectStream(timeout, std::move(writer), sent_last_message,
                                  options());
  };

  auto buffers = request.payload();
  do {
    std::size_t consumed = 0;
    for (auto const& b : buffers) {
      // flush_chunk() guarantees that maximum_chunk_size < chunk.size()
      auto capacity = maximum_chunk_size - chunk.size();
      if (capacity == 0) break;
      auto n = (std::min)(capacity, b.size());
      chunk.append(b.data(), b.data() + n);
      consumed += n;
    }
    PopFrontBytes(buffers, consumed);

    if (!flush_chunk(!buffers.empty())) return close_writer();
  } while (!buffers.empty());

  return close_writer();
}

StatusOr<ListBucketAclResponse> GrpcClient::ListBucketAcl(
    ListBucketAclRequest const& request) {
  auto get_request = GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  auto get = GetBucketMetadata(get_request);
  if (!get) return std::move(get).status();
  ListBucketAclResponse response;
  response.items = get->acl();
  return response;
}

StatusOr<BucketAccessControl> GrpcClient::GetBucketAcl(
    GetBucketAclRequest const& request) {
  auto get_request = GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  auto get = GetBucketMetadata(get_request);
  return FindBucketAccessControl(std::move(get), request.entity());
}

StatusOr<BucketAccessControl> GrpcClient::CreateBucketAcl(
    CreateBucketAclRequest const& request) {
  auto get_request = GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  auto updater = [&request](std::vector<BucketAccessControl> acl) {
    return UpsertAcl(std::move(acl), request.entity(), request.role());
  };
  return FindBucketAccessControl(
      ModifyBucketAccessControl(get_request, updater), request.entity());
}

StatusOr<EmptyResponse> GrpcClient::DeleteBucketAcl(
    DeleteBucketAclRequest const& request) {
  auto get_request = GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  auto updater = [&request](std::vector<BucketAccessControl> acl)
      -> StatusOr<std::vector<BucketAccessControl>> {
    auto i = std::remove_if(acl.begin(), acl.end(),
                            [&](BucketAccessControl const& entry) {
                              return entry.entity() == request.entity();
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
  return EmptyResponse{};
}

StatusOr<BucketAccessControl> GrpcClient::UpdateBucketAcl(
    UpdateBucketAclRequest const& request) {
  auto get_request = GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  auto updater = [&request](std::vector<BucketAccessControl> acl) {
    return UpsertAcl(std::move(acl), request.entity(), request.role());
  };
  return FindBucketAccessControl(
      ModifyBucketAccessControl(get_request, updater), request.entity());
}

StatusOr<BucketAccessControl> GrpcClient::PatchBucketAcl(
    PatchBucketAclRequest const& request) {
  auto get_request = GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  auto updater = [&request](std::vector<BucketAccessControl> acl) {
    return UpsertAcl(std::move(acl), request.entity(),
                     GrpcBucketAccessControlParser::Role(request.patch()));
  };
  return FindBucketAccessControl(
      ModifyBucketAccessControl(get_request, updater), request.entity());
}

StatusOr<ListObjectAclResponse> GrpcClient::ListObjectAcl(
    ListObjectAclRequest const& request) {
  auto get_request =
      GetObjectMetadataRequest(request.bucket_name(), request.object_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  auto get = GetObjectMetadata(get_request);
  if (!get) return std::move(get).status();
  ListObjectAclResponse response;
  response.items = get->acl();
  return response;
}

StatusOr<ObjectAccessControl> GrpcClient::CreateObjectAcl(
    CreateObjectAclRequest const& request) {
  auto get_request =
      GetObjectMetadataRequest(request.bucket_name(), request.object_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  auto updater = [&request](std::vector<ObjectAccessControl> acl) {
    return UpsertAcl(std::move(acl), request.entity(), request.role());
  };
  return FindObjectAccessControl(
      ModifyObjectAccessControl(get_request, updater), request.entity());
}

StatusOr<EmptyResponse> GrpcClient::DeleteObjectAcl(
    DeleteObjectAclRequest const& request) {
  auto get_request =
      GetObjectMetadataRequest(request.bucket_name(), request.object_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  auto updater = [&request](std::vector<ObjectAccessControl> acl)
      -> StatusOr<std::vector<ObjectAccessControl>> {
    auto i = std::remove_if(acl.begin(), acl.end(),
                            [&](ObjectAccessControl const& entry) {
                              return entry.entity() == request.entity();
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
  return EmptyResponse{};
}

StatusOr<ObjectAccessControl> GrpcClient::GetObjectAcl(
    GetObjectAclRequest const& request) {
  auto get_request =
      GetObjectMetadataRequest(request.bucket_name(), request.object_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  auto get = GetObjectMetadata(get_request);
  return FindObjectAccessControl(std::move(get), request.entity());
}

StatusOr<ObjectAccessControl> GrpcClient::UpdateObjectAcl(
    UpdateObjectAclRequest const& request) {
  auto get_request =
      GetObjectMetadataRequest(request.bucket_name(), request.object_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  auto updater = [&request](std::vector<ObjectAccessControl> acl) {
    return UpsertAcl(std::move(acl), request.entity(), request.role());
  };
  return FindObjectAccessControl(
      ModifyObjectAccessControl(get_request, updater), request.entity());
}

StatusOr<ObjectAccessControl> GrpcClient::PatchObjectAcl(
    PatchObjectAclRequest const& request) {
  auto get_request =
      GetObjectMetadataRequest(request.bucket_name(), request.object_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  auto updater = [&request](std::vector<ObjectAccessControl> acl) {
    return UpsertAcl(std::move(acl), request.entity(),
                     GrpcObjectAccessControlParser::Role(request.patch()));
  };
  return FindObjectAccessControl(
      ModifyObjectAccessControl(get_request, updater), request.entity());
}

StatusOr<ListDefaultObjectAclResponse> GrpcClient::ListDefaultObjectAcl(
    ListDefaultObjectAclRequest const& request) {
  auto get_request = GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  auto get = GetBucketMetadata(get_request);
  if (!get) return std::move(get).status();
  ListDefaultObjectAclResponse response;
  response.items = get->default_acl();
  return response;
}

StatusOr<ObjectAccessControl> GrpcClient::CreateDefaultObjectAcl(
    CreateDefaultObjectAclRequest const& request) {
  auto get_request = GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  auto updater = [&request](std::vector<ObjectAccessControl> acl) {
    return UpsertAcl(std::move(acl), request.entity(), request.role());
  };
  return FindDefaultObjectAccessControl(
      ModifyDefaultAccessControl(get_request, updater), request.entity());
}

StatusOr<EmptyResponse> GrpcClient::DeleteDefaultObjectAcl(
    DeleteDefaultObjectAclRequest const& request) {
  auto get_request = GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  auto updater = [&request](std::vector<ObjectAccessControl> acl)
      -> StatusOr<std::vector<ObjectAccessControl>> {
    auto i = std::remove_if(acl.begin(), acl.end(),
                            [&](ObjectAccessControl const& entry) {
                              return entry.entity() == request.entity();
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
  return EmptyResponse{};
}

StatusOr<ObjectAccessControl> GrpcClient::GetDefaultObjectAcl(
    GetDefaultObjectAclRequest const& request) {
  auto get_request = GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  auto get = GetBucketMetadata(get_request);
  if (!get) return std::move(get).status();
  return FindDefaultObjectAccessControl(std::move(get), request.entity());
}

StatusOr<ObjectAccessControl> GrpcClient::UpdateDefaultObjectAcl(
    UpdateDefaultObjectAclRequest const& request) {
  auto get_request = GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  auto updater = [&request](std::vector<ObjectAccessControl> acl) {
    return UpsertAcl(std::move(acl), request.entity(), request.role());
  };
  return FindDefaultObjectAccessControl(
      ModifyDefaultAccessControl(get_request, updater), request.entity());
}

StatusOr<ObjectAccessControl> GrpcClient::PatchDefaultObjectAcl(
    PatchDefaultObjectAclRequest const& request) {
  auto get_request = GetBucketMetadataRequest(request.bucket_name());
  request.ForEachOption(CopyCommonOptions(get_request));
  auto updater = [&request](std::vector<ObjectAccessControl> acl) {
    return UpsertAcl(std::move(acl), request.entity(),
                     GrpcObjectAccessControlParser::Role(request.patch()));
  };
  return FindDefaultObjectAccessControl(
      ModifyDefaultAccessControl(get_request, updater), request.entity());
}

StatusOr<ServiceAccount> GrpcClient::GetServiceAccount(
    GetProjectServiceAccountRequest const& request) {
  auto proto = GrpcServiceAccountParser::ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->GetServiceAccount(context, proto);
  if (!response) return std::move(response).status();
  return GrpcServiceAccountParser::FromProto(*response);
}

StatusOr<ListHmacKeysResponse> GrpcClient::ListHmacKeys(
    ListHmacKeysRequest const& request) {
  auto proto = GrpcHmacKeyRequestParser::ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->ListHmacKeys(context, proto);
  if (!response) return std::move(response).status();
  return GrpcHmacKeyRequestParser::FromProto(*response);
}

StatusOr<CreateHmacKeyResponse> GrpcClient::CreateHmacKey(
    CreateHmacKeyRequest const& request) {
  auto proto = GrpcHmacKeyRequestParser::ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->CreateHmacKey(context, proto);
  if (!response) return std::move(response).status();
  return GrpcHmacKeyRequestParser::FromProto(*response);
}

StatusOr<EmptyResponse> GrpcClient::DeleteHmacKey(
    DeleteHmacKeyRequest const& request) {
  auto proto = GrpcHmacKeyRequestParser::ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->DeleteHmacKey(context, proto);
  if (!response.ok()) return response;
  return EmptyResponse{};
}

StatusOr<HmacKeyMetadata> GrpcClient::GetHmacKey(
    GetHmacKeyRequest const& request) {
  auto proto = GrpcHmacKeyRequestParser::ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->GetHmacKey(context, proto);
  if (!response) return std::move(response).status();
  return GrpcHmacKeyMetadataParser::FromProto(*response);
}

StatusOr<HmacKeyMetadata> GrpcClient::UpdateHmacKey(
    UpdateHmacKeyRequest const& request) {
  auto proto = GrpcHmacKeyRequestParser::ToProto(request);
  grpc::ClientContext context;
  ApplyQueryParameters(context, request);
  auto response = stub_->UpdateHmacKey(context, proto);
  if (!response) return std::move(response).status();
  return GrpcHmacKeyMetadataParser::FromProto(*response);
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

StatusOr<BucketMetadata> GrpcClient::ModifyBucketAccessControl(
    GetBucketMetadataRequest const& request, BucketAclUpdater const& updater) {
  auto get = GetBucketMetadata(request);
  if (!get) return std::move(get).status();
  auto acl = updater(get->acl());
  if (!acl) return std::move(acl).status();
  auto patch = PatchBucket(
      PatchBucketRequest(request.bucket_name(),
                         BucketMetadataPatchBuilder().SetAcl(*std::move(acl)))
          .set_option(IfMetagenerationMatch(get->metageneration())));
  // Retry on failed preconditions
  if (patch.status().code() == StatusCode::kFailedPrecondition) {
    return Status(
        StatusCode::kUnavailable,
        "retrying BucketAccessControl change due to conflict, bucket=" +
            request.bucket_name());
  }
  return patch;
}

StatusOr<ObjectMetadata> GrpcClient::ModifyObjectAccessControl(
    GetObjectMetadataRequest const& request, ObjectAclUpdater const& updater) {
  auto get = GetObjectMetadata(request);
  if (!get) return std::move(get).status();
  auto acl = updater(get->acl());
  if (!acl) return std::move(acl).status();
  auto patch = PatchObject(
      PatchObjectRequest(request.bucket_name(), request.object_name(),
                         ObjectMetadataPatchBuilder().SetAcl(*std::move(acl)))
          .set_option(IfMetagenerationMatch(get->metageneration())));
  // Retry on failed preconditions
  if (patch.status().code() == StatusCode::kFailedPrecondition) {
    return Status(
        StatusCode::kUnavailable,
        "retrying ObjectAccessControl change due to conflict, bucket=" +
            request.bucket_name() + ", object=" + request.object_name());
  }
  return patch;
}

StatusOr<BucketMetadata> GrpcClient::ModifyDefaultAccessControl(
    GetBucketMetadataRequest const& request,
    DefaultObjectAclUpdater const& updater) {
  auto get = GetBucketMetadata(request);
  if (!get) return std::move(get).status();
  auto acl = updater(get->default_acl());
  if (!acl) return std::move(acl).status();
  auto patch = PatchBucket(
      PatchBucketRequest(
          request.bucket_name(),
          BucketMetadataPatchBuilder().SetDefaultAcl(*std::move(acl)))
          .set_option(IfMetagenerationMatch(get->metageneration())));
  // Retry on failed preconditions
  if (patch.status().code() == StatusCode::kFailedPrecondition) {
    return Status(StatusCode::kUnavailable,
                  "retrying DefaultObjectAccessControl change due to "
                  "conflict, bucket=" +
                      request.bucket_name());
  }
  return patch;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
