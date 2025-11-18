// Copyright 2023 Google LLC
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

#include "google/cloud/bigtable/internal/retry_traits.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/status_payload_keys.h"
#include <google/rpc/error_details.pb.h>
#include <google/rpc/status.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

/// @test Verify that certain known internal errors are retryable.
TEST(SafeGrpcRetry, RstStreamRetried) {
  EXPECT_FALSE(SafeGrpcRetry::IsTransientFailure(
      Status(StatusCode::kInternal, "non-retryable")));
  EXPECT_TRUE(SafeGrpcRetry::IsTransientFailure(
      Status(StatusCode::kInternal, "RST_STREAM")));
}

TEST(QueryPlanRefreshRetry, IsQueryPlanExpiredNoStatusPayload) {
  auto non_query_plan_failed_precondition =
      internal::FailedPreconditionError("not the query plan");
  EXPECT_FALSE(QueryPlanRefreshRetry::IsQueryPlanExpired(
      non_query_plan_failed_precondition));

  auto query_plan_expired =
      internal::FailedPreconditionError("PREPARED_QUERY_EXPIRED");
  EXPECT_TRUE(QueryPlanRefreshRetry::IsQueryPlanExpired(query_plan_expired));
}

TEST(QueryPlanRefreshRetry, QueryPlanExpiredStatusPayload) {
  auto query_plan_expired_violation =
      internal::FailedPreconditionError("failed precondition");
  google::rpc::PreconditionFailure_Violation violation;
  violation.set_type("PREPARED_QUERY_EXPIRED");
  violation.set_description(
      "The prepared query has expired. Please re-issue the ExecuteQuery with a "
      "valid prepared query.");
  google::rpc::PreconditionFailure precondition;
  *precondition.add_violations() = violation;
  google::rpc::Status status;
  status.set_code(9);
  status.set_message("failed precondition");
  google::protobuf::Any any;
  ASSERT_TRUE(any.PackFrom(precondition));
  *status.add_details() = any;
  internal::SetPayload(query_plan_expired_violation,
                       google::cloud::internal::StatusPayloadGrpcProto(),
                       status.SerializeAsString());
  EXPECT_TRUE(
      QueryPlanRefreshRetry::IsQueryPlanExpired(query_plan_expired_violation));
}

TEST(QueryPlanRefreshRetry, QueryPlanNotExpiredStatusPayload) {
  auto query_plan_not_expired_violation =
      internal::FailedPreconditionError("failed precondition");
  google::rpc::PreconditionFailure_Violation violation;
  violation.set_type("something else");
  violation.set_description("This is not the violation you are looking for");
  google::rpc::PreconditionFailure precondition;
  *precondition.add_violations() = violation;
  google::rpc::Status status;
  status.set_code(9);
  status.set_message("failed precondition");
  google::protobuf::Any any;
  ASSERT_TRUE(any.PackFrom(precondition));
  *status.add_details() = any;
  internal::SetPayload(query_plan_not_expired_violation,
                       google::cloud::internal::StatusPayloadGrpcProto(),
                       status.SerializeAsString());
  EXPECT_FALSE(QueryPlanRefreshRetry::IsQueryPlanExpired(
      query_plan_not_expired_violation));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
