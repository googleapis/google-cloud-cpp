// Copyright 2025 Google LLC
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

#include "google/cloud/bigtable/internal/query_plan.h"
#include "google/cloud/completion_queue.h"
#include <google/bigtable/v2/data.pb.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::shared_ptr<QueryPlan> QueryPlan::Create(
    CompletionQueue cq, google::bigtable::v2::PrepareQueryResponse response,
    RefreshFn fn) {
  auto plan = std::shared_ptr<QueryPlan>(
      new QueryPlan(std::move(cq), std::move(response), std::move(fn)));
  plan->Initialize();
  return plan;
}

bool QueryPlan::IsExpired() const { return false; }

StatusOr<std::string> QueryPlan::prepared_query() const {
  std::lock_guard<std::mutex> lock(mu_);
  if (IsExpired()) {
    return Status(StatusCode::kUnavailable, "Query plan has expired");
  }
  return response_.prepared_query();
}

StatusOr<google::bigtable::v2::ResultSetMetadata> QueryPlan::metadata() const {
  std::lock_guard<std::mutex> lock(mu_);
  if (IsExpired()) {
    return Status(StatusCode::kUnavailable, "Query plan has expired");
  }
  return response_.metadata();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
