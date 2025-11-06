// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/prepared_query.h"
#include "google/cloud/bigtable/sql_statement.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
using ::google::bigtable::v2::PrepareQueryResponse;

TEST(PreparedQuery, DefaultConstructor) {
  auto fake_cq_impl = std::make_shared<testing_util::FakeCompletionQueueImpl>();
  Project p("dummy-project");
  InstanceResource instance(p, "dummy-instance");
  std::string statement_contents(
      "SELECT * FROM my_table WHERE col1 = @val1 and col2 = @val2;");
  SqlStatement sql_statement(statement_contents);
  PrepareQueryResponse response;
  auto refresh_fn = [&response]() {
    return make_ready_future(
        StatusOr<google::bigtable::v2::PrepareQueryResponse>(response));
  };
  auto query_plan = bigtable_internal::QueryPlan::Create(
      CompletionQueue(fake_cq_impl), std::move(response),
      std::move(refresh_fn));
  PreparedQuery q(instance, sql_statement, query_plan);
  EXPECT_EQ(instance.FullName(), q.instance().FullName());
  EXPECT_EQ(statement_contents, q.sql_statement().sql());

  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
