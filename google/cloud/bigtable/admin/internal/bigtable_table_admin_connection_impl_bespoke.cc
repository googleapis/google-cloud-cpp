// Copyright 2026 Google LLC
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

#include "google/cloud/bigtable/admin/internal/bigtable_table_admin_connection_impl.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/async_retry_loop.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace bigtable_admin_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

std::unique_ptr<bigtable_admin::BigtableTableAdminRetryPolicy> retry_policy(
    Options const& options) {
  return options.get<bigtable_admin::BigtableTableAdminRetryPolicyOption>()
      ->clone();
}

std::unique_ptr<BackoffPolicy> backoff_policy(Options const& options) {
  return options.get<bigtable_admin::BigtableTableAdminBackoffPolicyOption>()
      ->clone();
}

std::unique_ptr<bigtable_admin::BigtableTableAdminConnectionIdempotencyPolicy>
idempotency_policy(Options const& options) {
  return options
      .get<
          bigtable_admin::BigtableTableAdminConnectionIdempotencyPolicyOption>()
      ->clone();
}

}  // namespace

future<StatusOr<google::bigtable::admin::v2::CheckConsistencyResponse>>
BigtableTableAdminConnectionImpl::WaitForConsistency(
    google::bigtable::admin::v2::CheckConsistencyRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto request_copy = request;
  auto const idempotent =
      idempotency_policy(*current)->CheckConsistency(request_copy);
  auto retry = retry_policy(*current);
  auto backoff = backoff_policy(*current);
  auto attempt_predicate =
      [](StatusOr<google::bigtable::admin::v2::CheckConsistencyResponse> const&
             r) { return r.ok() && r->consistent(); };
  return google::cloud::internal::AsyncRetryLoop(
      std::move(retry), std::move(backoff), idempotent, background_->cq(),
      [stub = stub_](
          CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
          google::cloud::internal::ImmutableOptions options,
          google::bigtable::admin::v2::CheckConsistencyRequest const& request) {
        return stub->AsyncCheckConsistency(cq, std::move(context),
                                           std::move(options), request);
      },
      std::move(current), std::move(request_copy), __func__,
      std::move(attempt_predicate));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_admin_internal
}  // namespace cloud
}  // namespace google
