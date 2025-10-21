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

using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::protobuf::util::TimeUtil;
using ::testing::IsEmpty;

TEST(QueryPlanTest, Accessors) {
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  CompletionQueue cq(mock_cq);
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
    return google::bigtable::v2::PrepareQueryResponse{};
  });

  EXPECT_EQ(plan->prepared_query(), "test-query");
  EXPECT_THAT(plan->metadata(),
              google::cloud::testing_util::IsProtoEqual(metadata));
}

TEST(QueryPlanTest, Expired) {
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  CompletionQueue cq(mock_cq);
  google::bigtable::v2::PrepareQueryResponse response;
  response.set_prepared_query("test-query");
  *response.mutable_metadata() = google::bigtable::v2::ResultSetMetadata{};

  // Set the expiration time to the past.
  *response.mutable_valid_until() =
      TimeUtil::GetCurrentTime() - TimeUtil::SecondsToDuration(1);

  auto plan = QueryPlan::Create(cq, response, [] {
    return google::bigtable::v2::PrepareQueryResponse{};
  });

  EXPECT_THAT(plan->prepared_query(), IsEmpty());
  EXPECT_THAT(plan->metadata(), google::cloud::testing_util::IsProtoEqual(
                                    google::bigtable::v2::ResultSetMetadata{}));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
