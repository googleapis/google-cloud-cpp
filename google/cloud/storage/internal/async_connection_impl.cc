// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async_connection_impl.h"
#include "google/cloud/storage/internal/async_accumulate_read_object.h"
#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/storage/internal/grpc_configure_client_context.h"
#include "google/cloud/storage/internal/grpc_object_request_parser.h"
#include "google/cloud/storage/internal/storage_stub_factory.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/internal/async_retry_loop.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

inline std::unique_ptr<storage::RetryPolicy> retry_policy() {
  return internal::CurrentOptions().get<storage::RetryPolicyOption>()->clone();
}

inline std::unique_ptr<BackoffPolicy> backoff_policy() {
  return internal::CurrentOptions()
      .get<storage::BackoffPolicyOption>()
      ->clone();
}

inline std::unique_ptr<storage::IdempotencyPolicy> idempotency_policy() {
  return internal::CurrentOptions()
      .get<storage::IdempotencyPolicyOption>()
      ->clone();
}

}  // namespace

AsyncConnectionImpl::AsyncConnectionImpl(CompletionQueue cq,
                                         std::shared_ptr<StorageStub> stub,
                                         Options options)
    : cq_(std::move(cq)),
      stub_(std::move(stub)),
      options_(std::move(options)) {}

future<storage_experimental::AsyncReadObjectRangeResponse>
AsyncConnectionImpl::AsyncReadObjectRange(
    storage::internal::ReadObjectRangeRequest request) {
  auto proto = ToProto(request);
  if (!proto) {
    auto response = storage_experimental::AsyncReadObjectRangeResponse{};
    response.status = std::move(proto).status();
    return make_ready_future(std::move(response));
  }

  auto context_factory = [request = std::move(request)]() {
    auto context = absl::make_unique<grpc::ClientContext>();
    ApplyQueryParameters(*context, request);
    return context;
  };
  auto const& current = internal::CurrentOptions();
  return storage_internal::AsyncAccumulateReadObjectFull(
             cq_, stub_, std::move(context_factory), *std::move(proto), current)
      .then([current](
                future<storage_internal::AsyncAccumulateReadObjectResult> f) {
        return ToResponse(f.get(), current);
      });
}

future<Status> AsyncConnectionImpl::AsyncDeleteObject(
    storage::internal::DeleteObjectRequest request) {
  auto proto = ToProto(request);
  auto const idempotency = idempotency_policy()->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return google::cloud::internal::AsyncRetryLoop(
      retry_policy(), backoff_policy(), idempotency, cq_,
      [stub = stub_, request = std::move(request)](
          CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
          google::storage::v2::DeleteObjectRequest const& proto) {
        ApplyQueryParameters(*context, request);
        return stub->AsyncDeleteObject(cq, std::move(context), proto);
      },
      proto, __func__);
}

future<StatusOr<std::string>> AsyncConnectionImpl::AsyncStartResumableWrite(
    storage::internal::ResumableUploadRequest request) {
  auto proto = ToProto(request);
  if (!proto) {
    return make_ready_future(StatusOr<std::string>(std::move(proto).status()));
  }
  // Always treat this as idempotent. See the `AsyncClient` documentation for
  // details.
  auto const idempotency = Idempotency::kIdempotent;
  return google::cloud::internal::AsyncRetryLoop(
             retry_policy(), backoff_policy(), idempotency, cq_,
             [stub = stub_, request = std::move(request)](
                 CompletionQueue& cq,
                 std::unique_ptr<grpc::ClientContext> context,
                 google::storage::v2::StartResumableWriteRequest const& proto) {
               ApplyQueryParameters(*context, request);
               return stub->AsyncStartResumableWrite(cq, std::move(context),
                                                     proto);
             },
             *std::move(proto), __func__)
      .then([](auto f) -> StatusOr<std::string> {
        auto response = f.get();
        if (!response) return std::move(response).status();
        return response->upload_id();
      });
}

std::shared_ptr<AsyncConnection> MakeAsyncConnection(CompletionQueue cq,
                                                     Options options) {
  options = storage::internal::DefaultOptionsGrpc(std::move(options));
  auto stub = CreateStorageStub(cq, options);
  return MakeAsyncConnection(std::move(cq), std::move(stub),
                             std::move(options));
}

std::shared_ptr<AsyncConnection> MakeAsyncConnection(
    CompletionQueue cq, std::shared_ptr<StorageStub> stub, Options options) {
  return std::make_shared<AsyncConnectionImpl>(std::move(cq), std::move(stub),
                                               std::move(options));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
