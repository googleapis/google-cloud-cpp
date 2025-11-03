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
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/fake_clock.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/bigtable/v2/data.pb.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/time_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::ToProtoTimestamp;
using ::google::cloud::testing_util::FakeCompletionQueueImpl;
using ::google::cloud::testing_util::FakeSystemClock;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::util::TimeUtil;

TEST(QueryPlanTest, ResponseDataWithOriginalValidQueryPlan) {
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();
  CompletionQueue cq(fake_cq_impl);

  google::bigtable::v2::PrepareQueryResponse response;
  response.set_prepared_query("test-query");
  auto constexpr kResultMetadataText = R"pb(
    proto_schema {
      columns {
        name: "user_id"
        type { string_type {} }
      }
    }
  )pb";
  google::bigtable::v2::ResultSetMetadata metadata;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kResultMetadataText,
                                                            &metadata));
  *response.mutable_metadata() = metadata;
  *response.mutable_valid_until() =
      TimeUtil::GetCurrentTime() + TimeUtil::SecondsToDuration(300);

  auto plan = QueryPlan::Create(cq, response, [] {
    return make_ready_future(
        make_status_or(google::bigtable::v2::PrepareQueryResponse{}));
  });

  auto response_data = plan->response();
  ASSERT_STATUS_OK(response_data);
  EXPECT_EQ(response_data->prepared_query(), "test-query");
  EXPECT_THAT(response_data->metadata(), IsProtoEqual(metadata));

  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}

TEST(QueryPlanTest, RefreshExpiredPlan) {
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();
  auto fake_clock = std::make_shared<FakeSystemClock>();
  auto now = std::chrono::system_clock::now();
  fake_clock->SetTime(now);

  google::bigtable::v2::PrepareQueryResponse refresh_response;
  refresh_response.set_prepared_query("refreshed-query-plan");
  auto refresh_fn = [&]() {
    return make_ready_future(make_status_or(refresh_response));
  };

  google::bigtable::v2::PrepareQueryResponse response;
  response.set_prepared_query("original-query-plan");
  *response.mutable_valid_until() =
      ToProtoTimestamp(now + std::chrono::seconds(600));

  auto query_plan = QueryPlan::Create(CompletionQueue(fake_cq_impl), response,
                                      refresh_fn, fake_clock);

  auto data = query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "original-query-plan");

  fake_clock->AdvanceTime(std::chrono::seconds(500));
  fake_cq_impl->SimulateCompletion(true);

  data = query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "refreshed-query-plan");

  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}

TEST(QueryPlanTest, FailedRefreshExpiredPlan) {
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();
  auto fake_clock = std::make_shared<FakeSystemClock>();
  auto now = std::chrono::system_clock::now();
  fake_clock->SetTime(now);

  auto failed_refresh = internal::InternalError("oops!");
  auto refresh_fn = [&]() {
    return make_ready_future(
        StatusOr<google::bigtable::v2::PrepareQueryResponse>(failed_refresh));
  };

  google::bigtable::v2::PrepareQueryResponse response;
  response.set_prepared_query("original-query-plan");
  *response.mutable_valid_until() =
      ToProtoTimestamp(now + std::chrono::seconds(600));

  auto query_plan = QueryPlan::Create(CompletionQueue(fake_cq_impl), response,
                                      refresh_fn, fake_clock);

  auto data = query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "original-query-plan");

  fake_clock->AdvanceTime(std::chrono::seconds(500));
  fake_cq_impl->SimulateCompletion(true);

  data = query_plan->response();
  EXPECT_THAT(data.status(), StatusIs(StatusCode::kInternal, "oops!"));

  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}

TEST(QueryPlanTest, RefreshInvalidatedPlan) {
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();
  auto fake_clock = std::make_shared<FakeSystemClock>();
  auto now = std::chrono::system_clock::now();
  fake_clock->SetTime(now);

  google::bigtable::v2::PrepareQueryResponse refresh_response;
  refresh_response.set_prepared_query("refreshed-query-plan");
  auto refresh_fn = [&]() {
    return make_ready_future(make_status_or(refresh_response));
  };

  google::bigtable::v2::PrepareQueryResponse response;
  response.set_prepared_query("original-query-plan");
  *response.mutable_valid_until() =
      ToProtoTimestamp(now + std::chrono::seconds(600));

  auto query_plan = QueryPlan::Create(CompletionQueue(fake_cq_impl), response,
                                      refresh_fn, fake_clock);

  auto data = query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "original-query-plan");

  auto invalid_status = internal::InternalError("oops!");
  query_plan->Invalidate(invalid_status, data->prepared_query());

  data = query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "refreshed-query-plan");

  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}

TEST(QueryPlanTest, FailedRefreshInvalidatedPlan) {
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();
  auto fake_clock = std::make_shared<FakeSystemClock>();
  auto now = std::chrono::system_clock::now();
  fake_clock->SetTime(now);

  auto failed_refresh = internal::InternalError("oops again!");
  auto refresh_fn = [&]() {
    return make_ready_future(
        StatusOr<google::bigtable::v2::PrepareQueryResponse>(failed_refresh));
  };

  google::bigtable::v2::PrepareQueryResponse response;
  response.set_prepared_query("original-query-plan");
  *response.mutable_valid_until() =
      ToProtoTimestamp(now + std::chrono::seconds(600));

  auto query_plan = QueryPlan::Create(CompletionQueue(fake_cq_impl), response,
                                      refresh_fn, fake_clock);

  auto data = query_plan->response();
  ASSERT_STATUS_OK(data);
  EXPECT_EQ(data->prepared_query(), "original-query-plan");

  auto invalid_status = internal::InternalError("oops!");
  query_plan->Invalidate(invalid_status, data->prepared_query());

  data = query_plan->response();
  EXPECT_THAT(data.status(), StatusIs(StatusCode::kInternal, "oops again!"));

  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
