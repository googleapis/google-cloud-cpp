// Copyright 2022 Google LLC
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

#include "google/cloud/pubsublite/internal/topic_partition_count_reader_impl.h"
#include "google/cloud/pubsublite/mocks/mock_admin_connection.h"
#include "google/cloud/future.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

using ::google::cloud::testing_util::IsProtoEqual;

using google::cloud::pubsublite::Topic;
using google::cloud::pubsublite::v1::GetTopicPartitionsRequest;
using google::cloud::pubsublite::v1::TopicPartitions;
using google::cloud::pubsublite_mocks::MockAdminServiceConnection;

TEST(TestAsyncRead, Valid) {
  std::uint32_t const num_partitions = 50;
  auto connection = std::make_shared<MockAdminServiceConnection>();
  TopicPartitionCountReaderImpl reader{connection};
  Topic topic{"project", "location", "name"};
  GetTopicPartitionsRequest req;
  *req.mutable_name() = topic.FullName();
  promise<void> request_blocker;
  EXPECT_CALL(*connection, GetTopicPartitions(IsProtoEqual(req)))
      .WillOnce([&](GetTopicPartitionsRequest const&) {
        request_blocker.get_future().get();
        TopicPartitions tp;
        tp.set_partition_count(num_partitions);
        return tp;
      });
  future<StatusOr<std::uint32_t>> f = reader.Read(topic);
  EXPECT_EQ(f.wait_for(std::chrono::seconds(2)), std::future_status::timeout);
  request_blocker.set_value();
  auto val = f.get();
  EXPECT_TRUE(val);
  EXPECT_EQ(*val, num_partitions);
}

TEST(TestAsyncRead, PartitionReadStatusError) {
  Status error_status = Status{StatusCode::kAborted, "123"};
  auto connection = std::make_shared<MockAdminServiceConnection>();
  TopicPartitionCountReaderImpl reader{connection};
  Topic topic{"project1", "location1", "name1"};
  GetTopicPartitionsRequest req;
  *req.mutable_name() = topic.FullName();
  promise<void> request_blocker;
  EXPECT_CALL(*connection, GetTopicPartitions(IsProtoEqual(req)))
      .WillOnce([&](GetTopicPartitionsRequest const&) {
        request_blocker.get_future().get();
        return error_status;
      });
  future<StatusOr<std::uint32_t>> f = reader.Read(topic);
  EXPECT_EQ(f.wait_for(std::chrono::seconds(2)), std::future_status::timeout);
  request_blocker.set_value();
  auto val = f.get();
  EXPECT_FALSE(val);
  EXPECT_EQ(val.status(), error_status);
}

TEST(TestAsyncRead, PartitionReadPartitionCountError) {
  std::int64_t const num_partitions = UINT32_MAX + static_cast<std::int64_t>(1);
  auto connection = std::make_shared<MockAdminServiceConnection>();
  TopicPartitionCountReaderImpl reader{connection};
  Topic topic{"project", "location", "name"};
  GetTopicPartitionsRequest req;
  *req.mutable_name() = topic.FullName();
  promise<void> request_blocker;
  EXPECT_CALL(*connection, GetTopicPartitions(IsProtoEqual(req)))
      .WillOnce([&](GetTopicPartitionsRequest const&) {
        request_blocker.get_future().get();
        TopicPartitions tp;
        tp.set_partition_count(num_partitions);
        return tp;
      });
  future<StatusOr<std::uint32_t>> f = reader.Read(topic);
  EXPECT_EQ(f.wait_for(std::chrono::seconds(2)), std::future_status::timeout);
  request_blocker.set_value();
  auto val = f.get();
  EXPECT_FALSE(val);
  EXPECT_EQ(val.status(), Status(StatusCode::kFailedPrecondition,
                                 "Returned partition count is too big: " +
                                     std::to_string(num_partitions)));
}

TEST(TestAsyncRead, PartitionReadPartitionCountErrorBoundary) {
  std::int64_t const num_partitions = UINT32_MAX;
  auto connection = std::make_shared<MockAdminServiceConnection>();
  TopicPartitionCountReaderImpl reader{connection};
  Topic topic{"project", "location", "name"};
  GetTopicPartitionsRequest req;
  *req.mutable_name() = topic.FullName();
  promise<void> request_blocker;
  EXPECT_CALL(*connection, GetTopicPartitions(IsProtoEqual(req)))
      .WillOnce([&](GetTopicPartitionsRequest const&) {
        request_blocker.get_future().get();
        TopicPartitions tp;
        tp.set_partition_count(num_partitions);
        return tp;
      });
  future<StatusOr<std::uint32_t>> f = reader.Read(topic);
  EXPECT_EQ(f.wait_for(std::chrono::seconds(2)), std::future_status::timeout);
  request_blocker.set_value();
  auto val = f.get();
  EXPECT_FALSE(val);
  EXPECT_EQ(val.status(), Status(StatusCode::kFailedPrecondition,
                                 "Returned partition count is too big: " +
                                     std::to_string(num_partitions)));
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
