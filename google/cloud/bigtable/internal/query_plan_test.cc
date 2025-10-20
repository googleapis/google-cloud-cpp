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
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

/// @test Basic test to confirm compilation and dummy behavior.
/// Remove this after implementing QueryPlan.
TEST(QueryPlanTest, TestDummyMethods) {
  auto cq = CompletionQueue();
  auto response = google::bigtable::v2::PrepareQueryResponse();
  auto fn = QueryPlan::RefreshFn();
  auto created = QueryPlan::Create(cq, response, fn);
  ASSERT_EQ("", (*created).prepared_query());
  ASSERT_EQ(0, created->metadata().ByteSizeLong());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
