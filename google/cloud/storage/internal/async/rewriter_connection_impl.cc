// Copyright 2024 Google LLC
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

#include "google/cloud/storage/internal/async/rewriter_connection_impl.h"
#include "google/cloud/storage/internal/grpc/configure_client_context.h"
#include "google/cloud/storage/internal/grpc/object_metadata_parser.h"
#include "google/cloud/storage/internal/grpc/object_request_parser.h"
#include "google/cloud/internal/async_retry_loop.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

StatusOr<storage_experimental::RewriteObjectResponse> CannotLockSelf(
    internal::ErrorInfoBuilder eib) {
  return internal::CancelledError("cannot lock self", std::move(eib));
}

}  // namespace

RewriterConnectionImpl::RewriterConnectionImpl(
    CompletionQueue cq, std::shared_ptr<StorageStub> stub,
    google::cloud::internal::ImmutableOptions current,
    storage::internal::RewriteObjectRequest request)
    : cq_(std::move(cq)),
      stub_(std::move(stub)),
      current_(std::move(current)),
      request_(std::move(request)) {}

future<StatusOr<storage_experimental::RewriteObjectResponse>>
RewriterConnectionImpl::Iterate() {
  auto proto = ToProto(request_);
  if (!proto) {
    return make_ready_future(
        StatusOr<storage_experimental::RewriteObjectResponse>(
            std::move(proto).status()));
  }

  auto const idempotency =
      current_->get<storage::IdempotencyPolicyOption>()->clone()->IsIdempotent(
          request_)
          ? Idempotency::kIdempotent
          : Idempotency::kNonIdempotent;
  auto call = [stub = stub_, current = current_, request = request_](
                  CompletionQueue& cq,
                  std::shared_ptr<grpc::ClientContext> context,
                  google::storage::v2::RewriteObjectRequest const& proto) {
    ApplyQueryParameters(*context, *current, request);
    internal::OptionsSpan span(*current);
    return stub->AsyncRewriteObject(cq, std::move(context), proto);
  };

  return google::cloud::internal::AsyncRetryLoop(
             current_->get<storage::RetryPolicyOption>()->clone(),
             current_->get<storage::BackoffPolicyOption>()->clone(),
             idempotency, cq_, std::move(call), *std::move(proto), __func__)
      .then([w = WeakFromThis()](auto f) {
        if (auto self = w.lock()) return self->OnRewrite(f.get());
        return CannotLockSelf(GCP_ERROR_INFO());
      });
}

StatusOr<storage_experimental::RewriteObjectResponse>
RewriterConnectionImpl::OnRewrite(
    StatusOr<google::storage::v2::RewriteResponse> response) {
  if (!response) return std::move(response).status();
  request_.set_rewrite_token(response->rewrite_token());
  auto r = storage_experimental::RewriteObjectResponse{
      response->total_bytes_rewritten(),
      response->object_size(),
      response->rewrite_token(),
      /*.metadata=*/absl::nullopt,
  };
  if (response->has_resource()) {
    r.metadata = FromProto(response->resource(), *current_);
  }
  return r;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
