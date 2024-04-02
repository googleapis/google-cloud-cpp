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
#include "google/cloud/storage/async/idempotency_policy.h"
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

StatusOr<google::storage::v2::RewriteResponse> CannotLockSelf(
    internal::ErrorInfoBuilder eib) {
  return internal::CancelledError("cannot lock self", std::move(eib));
}

}  // namespace

RewriterConnectionImpl::RewriterConnectionImpl(
    CompletionQueue cq, std::shared_ptr<StorageStub> stub,
    google::cloud::internal::ImmutableOptions current,
    google::storage::v2::RewriteObjectRequest request)
    : cq_(std::move(cq)),
      stub_(std::move(stub)),
      current_(std::move(current)),
      request_(std::move(request)) {}

future<StatusOr<google::storage::v2::RewriteResponse>>
RewriterConnectionImpl::Iterate() {
  auto policy =
      current_->get<storage_experimental::IdempotencyPolicyOption>()();
  return google::cloud::internal::AsyncRetryLoop(
             current_->get<storage::RetryPolicyOption>()->clone(),
             current_->get<storage::BackoffPolicyOption>()->clone(),
             policy->RewriteObject(request_), cq_,
             [stub = stub_](
                 CompletionQueue& cq,
                 std::shared_ptr<grpc::ClientContext> context,
                 google::cloud::internal::ImmutableOptions options,
                 google::storage::v2::RewriteObjectRequest const& proto) {
               return stub->AsyncRewriteObject(cq, std::move(context),
                                               std::move(options), proto);
             },
             current_, request_, __func__)
      .then([w = WeakFromThis()](auto f) {
        if (auto self = w.lock()) return self->OnRewrite(f.get());
        return CannotLockSelf(GCP_ERROR_INFO());
      });
}

StatusOr<google::storage::v2::RewriteResponse>
RewriterConnectionImpl::OnRewrite(
    StatusOr<google::storage::v2::RewriteResponse> response) {
  if (!response) return std::move(response).status();
  request_.set_rewrite_token(response->rewrite_token());
  return response;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
