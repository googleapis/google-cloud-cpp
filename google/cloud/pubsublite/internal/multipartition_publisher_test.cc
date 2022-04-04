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

#include "google/cloud/pubsublite/internal/multipartition_publisher.h"
#include "google/cloud/pubsublite/mocks/mock_admin_connection.h"
#include "google/cloud/pubsublite/testing/mock_alarm_registry.h"
#include "google/cloud/pubsublite/testing/mock_partition_publisher.h"
#include "google/cloud/pubsublite/testing/mock_routing_policy.h"
#include "google/cloud/testing_util/is_proto_equal.h"

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using Partition = RoutingPolicy::Partition;
using google::cloud::pubsublite::MessageMetadata;
using google::cloud::pubsublite_mocks::MockAdminServiceConnection;

using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::_;
using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

using google::cloud::pubsublite::v1::Cursor;
using google::cloud::pubsublite::v1::GetTopicPartitionsRequest;
using google::cloud::pubsublite::v1::TopicPartitions;

using google::cloud::pubsublite::Topic;

using ::google::cloud::pubsublite_testing::MockAlarmRegistry;
using ::google::cloud::pubsublite_testing::MockAlarmRegistryCancelToken;
using ::google::cloud::pubsublite_testing::MockPartitionPublisher;
using ::google::cloud::pubsublite_testing::MockRoutingPolicy;

auto const kAlarmDuration = std::chrono::milliseconds{std::chrono::seconds{60}};

class SinglePublisherTest : public ::testing::Test {
 protected:
  SinglePublisherTest()
      : partition_publisher_ref_{*(new StrictMock<MockPartitionPublisher>())},
        admin_connection_{
            std::make_shared<StrictMock<MockAdminServiceConnection>>()},
        alarm_token_ref_{*(new StrictMock<MockAlarmRegistryCancelToken>())},
        routing_policy_ref_{*(new StrictMock<MockRoutingPolicy>())},
        topic_{"project", "location", "name"},
        topic_partitions_request_{[&] {
          GetTopicPartitionsRequest req;
          *req.mutable_name() = topic_.FullName();
          return req;
        }()},
        topic_partitions_response_{[] {
          TopicPartitions tp;
          tp.set_partition_count(1);
          return tp;
        }()} {
    EXPECT_CALL(alarm_registry_, RegisterAlarm(kAlarmDuration, _))
        .WillOnce(WithArg<1>([&](std::function<void()> on_alarm) {
          // as this is a unit test, we mock the AlarmRegistry behavior
          // this enables the test suite to control when the alarm is
          // rung/messages are flushed
          on_alarm_ = std::move(on_alarm);
          return absl::WrapUnique(&alarm_token_ref_);
        }));

    multipartition_publisher_ = absl::make_unique<MultipartitionPublisher>(
        partition_publisher_factory_.AsStdFunction(), admin_connection_,
        alarm_registry_, absl::WrapUnique(&routing_policy_ref_), topic_);
  }

  StrictMock<MockFunction<std::unique_ptr<Publisher<Cursor>>(Partition)>>
      partition_publisher_factory_;
  StrictMock<MockPartitionPublisher>& partition_publisher_ref_;
  std::shared_ptr<StrictMock<MockAdminServiceConnection>> admin_connection_;
  StrictMock<MockAlarmRegistry> alarm_registry_;
  StrictMock<MockAlarmRegistryCancelToken>& alarm_token_ref_;
  StrictMock<MockRoutingPolicy>& routing_policy_ref_;
  Topic const topic_;
  GetTopicPartitionsRequest const topic_partitions_request_;
  TopicPartitions topic_partitions_response_;
  std::function<void()> on_alarm_;
  std::unique_ptr<Publisher<MessageMetadata>> multipartition_publisher_;
};

TEST_F(SinglePublisherTest, PublisherCreatedFromStartGood) {
  InSequence seq;

  promise<StatusOr<TopicPartitions>> num_partitions;
  EXPECT_CALL(*admin_connection_,
              AsyncGetTopicPartitions(IsProtoEqual(topic_partitions_request_)))
      .WillOnce(Return(ByMove(num_partitions.get_future())));

  auto start = multipartition_publisher_->Start();

  EXPECT_CALL(partition_publisher_factory_, Call(0))
      .WillOnce(Return(ByMove(absl::WrapUnique(&partition_publisher_ref_))));
  promise<Status> partition_publisher_start;
  EXPECT_CALL(partition_publisher_ref_, Start)
      .WillOnce(Return(ByMove(partition_publisher_start.get_future())));

  num_partitions.set_value(topic_partitions_response_);

  EXPECT_CALL(alarm_token_ref_, Destroy);
  EXPECT_CALL(partition_publisher_ref_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));

  multipartition_publisher_->Shutdown();

  EXPECT_EQ(start.get(), Status());
}

TEST_F(SinglePublisherTest, PublisherCreatedFromAlarmGood) {
  InSequence seq;

  promise<StatusOr<TopicPartitions>> num_partitions;
  EXPECT_CALL(*admin_connection_,
              AsyncGetTopicPartitions(IsProtoEqual(topic_partitions_request_)))
      .WillOnce(Return(ByMove(num_partitions.get_future())));
  auto start = multipartition_publisher_->Start();

  promise<StatusOr<TopicPartitions>> num_partitions1;
  EXPECT_CALL(*admin_connection_,
              AsyncGetTopicPartitions(IsProtoEqual(topic_partitions_request_)))
      .WillOnce(Return(ByMove(num_partitions1.get_future())));
  on_alarm_();

  EXPECT_CALL(partition_publisher_factory_, Call(0))
      .WillOnce(Return(ByMove(absl::WrapUnique(&partition_publisher_ref_))));
  promise<Status> partition_publisher_start;
  EXPECT_CALL(partition_publisher_ref_, Start)
      .WillOnce(Return(ByMove(partition_publisher_start.get_future())));

  num_partitions1.set_value(topic_partitions_response_);
  num_partitions.set_value(topic_partitions_response_);

  EXPECT_CALL(alarm_token_ref_, Destroy);
  EXPECT_CALL(partition_publisher_ref_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));

  multipartition_publisher_->Shutdown();

  EXPECT_EQ(start.get(), Status());
}

TEST_F(SinglePublisherTest, PublisherCreatedFromStartGoodAlarmFail) {
  InSequence seq;

  // doesn't invoke `AsyncGetTopicPartitions` b/c not `Start`ed yet
  on_alarm_();

  promise<StatusOr<TopicPartitions>> num_partitions;
  EXPECT_CALL(*admin_connection_,
              AsyncGetTopicPartitions(IsProtoEqual(topic_partitions_request_)))
      .WillOnce(Return(ByMove(num_partitions.get_future())));
  auto start = multipartition_publisher_->Start();

  EXPECT_CALL(partition_publisher_factory_, Call(0))
      .WillOnce(Return(ByMove(absl::WrapUnique(&partition_publisher_ref_))));
  promise<Status> partition_publisher_start;
  EXPECT_CALL(partition_publisher_ref_, Start)
      .WillOnce(Return(ByMove(partition_publisher_start.get_future())));

  num_partitions.set_value(topic_partitions_response_);

  EXPECT_CALL(alarm_token_ref_, Destroy);
  EXPECT_CALL(partition_publisher_ref_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));

  multipartition_publisher_->Shutdown();

  EXPECT_EQ(start.get(), Status());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google
