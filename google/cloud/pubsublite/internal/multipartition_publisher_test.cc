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
#include <limits>

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::pubsublite::MessageMetadata;
using ::google::cloud::pubsublite_mocks::MockAdminServiceConnection;

using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::_;
using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

using ::google::cloud::pubsublite::v1::Cursor;
using ::google::cloud::pubsublite::v1::GetTopicPartitionsRequest;
using ::google::cloud::pubsublite::v1::PubSubMessage;
using ::google::cloud::pubsublite::v1::TopicPartitions;

using ::google::cloud::pubsublite::Topic;

using ::google::cloud::pubsublite_testing::MockAlarmRegistry;
using ::google::cloud::pubsublite_testing::MockAlarmRegistryCancelToken;
using ::google::cloud::pubsublite_testing::MockPartitionPublisher;
using ::google::cloud::pubsublite_testing::MockRoutingPolicy;

constexpr std::chrono::milliseconds kAlarmDuration{std::chrono::seconds{60}};
constexpr std::int64_t kOutOfBoundsPartition =
    static_cast<std::int64_t>(std::numeric_limits<Partition>::max()) + 1;
constexpr std::int64_t kOffsetPlaceholder = 42;

Topic ExampleTopic() { return Topic{"project", "location", "name"}; }

GetTopicPartitionsRequest ExamplePartitionsRequest() {
  GetTopicPartitionsRequest req;
  *req.mutable_name() = ExampleTopic().FullName();
  return req;
}

TopicPartitions ExamplePartitionsResponse(std::int64_t partition_count) {
  TopicPartitions tp;
  tp.set_partition_count(partition_count);
  return tp;
}

future<StatusOr<TopicPartitions>> ReadyTopicPartitionsFuture(
    std::int64_t partition_count) {
  return make_ready_future(
      make_status_or(ExamplePartitionsResponse(partition_count)));
}

class MultipartitionPublisherNoneInitializedTest : public ::testing::Test {
 protected:
  MultipartitionPublisherNoneInitializedTest()
      : admin_connection_{std::make_shared<
            StrictMock<MockAdminServiceConnection>>()},
        alarm_token_{*(new StrictMock<MockAlarmRegistryCancelToken>())},
        routing_policy_{*(new StrictMock<MockRoutingPolicy>())} {
    EXPECT_CALL(alarm_registry_, RegisterAlarm(kAlarmDuration, _))
        .WillOnce(WithArg<1>([&](std::function<void()> on_alarm) {
          on_alarm_ = std::move(on_alarm);
          return absl::WrapUnique(&alarm_token_);
        }));

    multipartition_publisher_ = absl::make_unique<MultipartitionPublisher>(
        partition_publisher_factory_.AsStdFunction(), admin_connection_,
        alarm_registry_, absl::WrapUnique(&routing_policy_), ExampleTopic());
  }

  StrictMock<MockFunction<std::shared_ptr<Publisher<Cursor>>(Partition)>>
      partition_publisher_factory_;
  std::shared_ptr<StrictMock<MockAdminServiceConnection>> admin_connection_;
  StrictMock<MockAlarmRegistry> alarm_registry_;
  StrictMock<MockAlarmRegistryCancelToken>& alarm_token_;
  StrictMock<MockRoutingPolicy>& routing_policy_;
  std::function<void()> on_alarm_;
  std::unique_ptr<Publisher<MessageMetadata>> multipartition_publisher_;
};

TEST_F(MultipartitionPublisherNoneInitializedTest, StartNotCalled) {
  EXPECT_CALL(alarm_token_, Destroy);
}

TEST_F(MultipartitionPublisherNoneInitializedTest,
       FirstPollInvalidValuePublisherAborts) {
  InSequence seq;

  EXPECT_CALL(*admin_connection_,
              AsyncGetTopicPartitions(IsProtoEqual(ExamplePartitionsRequest())))
      .WillOnce(
          Return(ByMove(ReadyTopicPartitionsFuture(kOutOfBoundsPartition))));
  auto start = multipartition_publisher_->Start();

  EXPECT_EQ(start.get(), Status(StatusCode::kInternal,
                                "Returned partition count is too big: " +
                                    std::to_string(kOutOfBoundsPartition)));

  EXPECT_CALL(alarm_token_, Destroy);
  multipartition_publisher_->Shutdown().get();
}

TEST_F(MultipartitionPublisherNoneInitializedTest,
       PublishAndShutdownBeforePublisherCreated) {
  InSequence seq;

  promise<StatusOr<TopicPartitions>> num_partitions;
  EXPECT_CALL(*admin_connection_,
              AsyncGetTopicPartitions(IsProtoEqual(ExamplePartitionsRequest())))
      .WillOnce(Return(ByMove(num_partitions.get_future())));

  auto start = multipartition_publisher_->Start();

  PubSubMessage m0;
  *m0.mutable_data() = "data1";
  future<StatusOr<MessageMetadata>> message0 =
      multipartition_publisher_->Publish(m0);
  PubSubMessage m1;
  *m1.mutable_data() = "data2";
  future<StatusOr<MessageMetadata>> message1 =
      multipartition_publisher_->Publish(m1);

  EXPECT_CALL(alarm_token_, Destroy);

  multipartition_publisher_->Shutdown();

  num_partitions.set_value(
      ExamplePartitionsResponse(1));  // shouldn't do anything

  EXPECT_EQ(message0.get().status(),
            Status(StatusCode::kFailedPrecondition,
                   "Multipartition publisher shutdown."));
  EXPECT_EQ(message1.get().status(),
            Status(StatusCode::kFailedPrecondition,
                   "Multipartition publisher shutdown."));

  EXPECT_EQ(start.get(), Status());
}

class MultipartitionPublisherTest
    : public MultipartitionPublisherNoneInitializedTest {
 protected:
  MultipartitionPublisherTest()
      : partition_publisher_0_{std::make_shared<
            StrictMock<MockPartitionPublisher>>()},
        partition_publisher_1_{
            std::make_shared<StrictMock<MockPartitionPublisher>>()} {}

  std::shared_ptr<StrictMock<MockPartitionPublisher>> partition_publisher_0_;
  std::shared_ptr<StrictMock<MockPartitionPublisher>> partition_publisher_1_;
};

TEST_F(MultipartitionPublisherTest, PublisherCreatedFromStartGood) {
  InSequence seq;

  promise<StatusOr<TopicPartitions>> num_partitions;
  EXPECT_CALL(*admin_connection_,
              AsyncGetTopicPartitions(IsProtoEqual(ExamplePartitionsRequest())))
      .WillOnce(Return(ByMove(num_partitions.get_future())));
  auto start = multipartition_publisher_->Start();

  EXPECT_CALL(partition_publisher_factory_, Call(0))
      .WillOnce(Return(partition_publisher_0_));
  promise<Status> partition_publisher_start_0;
  EXPECT_CALL(*partition_publisher_0_, Start)
      .WillOnce(Return(ByMove(partition_publisher_start_0.get_future())));
  EXPECT_CALL(partition_publisher_factory_, Call(1))
      .WillOnce(Return(partition_publisher_1_));
  promise<Status> partition_publisher_start_1;
  EXPECT_CALL(*partition_publisher_1_, Start)
      .WillOnce(Return(ByMove(partition_publisher_start_1.get_future())));

  num_partitions.set_value(ExamplePartitionsResponse(2));

  EXPECT_CALL(alarm_token_, Destroy);
  EXPECT_CALL(*partition_publisher_0_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  EXPECT_CALL(*partition_publisher_1_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  multipartition_publisher_->Shutdown();

  partition_publisher_start_0.set_value(Status());
  partition_publisher_start_1.set_value(Status());

  EXPECT_EQ(start.get(), Status());

  // https://stackoverflow.com/questions/10286514/why-is-googlemock-leaking-my-shared-ptr
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(partition_publisher_0_.get()));
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(partition_publisher_1_.get()));
}

TEST_F(MultipartitionPublisherTest, StartRunsOnAlarmDoesNot) {
  InSequence seq;

  promise<StatusOr<TopicPartitions>> num_partitions;
  EXPECT_CALL(*admin_connection_,
              AsyncGetTopicPartitions(IsProtoEqual(ExamplePartitionsRequest())))
      .WillOnce(Return(ByMove(num_partitions.get_future())));
  auto start = multipartition_publisher_->Start();

  on_alarm_();  // doesn't do anything because `Start` already invoked
                // TriggerPublisherCreation and is waiting on it

  EXPECT_CALL(partition_publisher_factory_, Call(0))
      .WillOnce(Return(partition_publisher_0_));
  promise<Status> partition_publisher_start_0;
  EXPECT_CALL(*partition_publisher_0_, Start)
      .WillOnce(Return(ByMove(partition_publisher_start_0.get_future())));
  EXPECT_CALL(partition_publisher_factory_, Call(1))
      .WillOnce(Return(partition_publisher_1_));
  promise<Status> partition_publisher_start_1;
  EXPECT_CALL(*partition_publisher_1_, Start)
      .WillOnce(Return(ByMove(partition_publisher_start_1.get_future())));

  num_partitions.set_value(ExamplePartitionsResponse(2));

  EXPECT_CALL(alarm_token_, Destroy);
  EXPECT_CALL(*partition_publisher_0_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  EXPECT_CALL(*partition_publisher_1_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  multipartition_publisher_->Shutdown();

  partition_publisher_start_0.set_value(Status());
  partition_publisher_start_1.set_value(Status());

  EXPECT_EQ(start.get(), Status());

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(partition_publisher_0_.get()));
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(partition_publisher_1_.get()));
}

TEST_F(MultipartitionPublisherTest, PublisherCreatedFromStartGoodAlarmFail) {
  InSequence seq;

  // doesn't invoke `AsyncGetTopicPartitions` b/c not `Start`ed yet
  EXPECT_CALL(*admin_connection_,
              AsyncGetTopicPartitions(IsProtoEqual(ExamplePartitionsRequest())))
      .WillOnce(Return(ByMove(ReadyTopicPartitionsFuture(1))));
  on_alarm_();

  promise<StatusOr<TopicPartitions>> num_partitions;
  EXPECT_CALL(*admin_connection_,
              AsyncGetTopicPartitions(IsProtoEqual(ExamplePartitionsRequest())))
      .WillOnce(Return(ByMove(num_partitions.get_future())));
  auto start = multipartition_publisher_->Start();

  EXPECT_CALL(partition_publisher_factory_, Call(0))
      .WillOnce(Return(partition_publisher_0_));
  promise<Status> partition_publisher_start_0;
  EXPECT_CALL(*partition_publisher_0_, Start)
      .WillOnce(Return(ByMove(partition_publisher_start_0.get_future())));
  EXPECT_CALL(partition_publisher_factory_, Call(1))
      .WillOnce(Return(partition_publisher_1_));
  promise<Status> partition_publisher_start_1;
  EXPECT_CALL(*partition_publisher_1_, Start)
      .WillOnce(Return(ByMove(partition_publisher_start_1.get_future())));

  num_partitions.set_value(ExamplePartitionsResponse(2));

  EXPECT_CALL(alarm_token_, Destroy);
  EXPECT_CALL(*partition_publisher_0_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  EXPECT_CALL(*partition_publisher_1_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  multipartition_publisher_->Shutdown();

  partition_publisher_start_0.set_value(Status());
  partition_publisher_start_1.set_value(Status());

  EXPECT_EQ(start.get(), Status());

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(partition_publisher_0_.get()));
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(partition_publisher_1_.get()));
}

TEST_F(MultipartitionPublisherTest, ShutdownWhileGettingNumPartitions) {
  InSequence seq;

  promise<StatusOr<TopicPartitions>> num_partitions_0;
  EXPECT_CALL(*admin_connection_,
              AsyncGetTopicPartitions(IsProtoEqual(ExamplePartitionsRequest())))
      .WillOnce(Return(ByMove(num_partitions_0.get_future())));
  auto start = multipartition_publisher_->Start();

  EXPECT_CALL(partition_publisher_factory_, Call(0))
      .WillOnce(Return(partition_publisher_0_));
  promise<Status> partition_publisher_start_0;
  EXPECT_CALL(*partition_publisher_0_, Start)
      .WillOnce(Return(ByMove(partition_publisher_start_0.get_future())));
  EXPECT_CALL(partition_publisher_factory_, Call(1))
      .WillOnce(Return(partition_publisher_1_));
  promise<Status> partition_publisher_start_1;
  EXPECT_CALL(*partition_publisher_1_, Start)
      .WillOnce(Return(ByMove(partition_publisher_start_1.get_future())));

  num_partitions_0.set_value(ExamplePartitionsResponse(2));

  promise<StatusOr<TopicPartitions>> num_partitions_1;
  EXPECT_CALL(*admin_connection_,
              AsyncGetTopicPartitions(IsProtoEqual(ExamplePartitionsRequest())))
      .WillOnce(Return(ByMove(num_partitions_1.get_future())));
  on_alarm_();

  EXPECT_CALL(alarm_token_, Destroy);
  EXPECT_CALL(*partition_publisher_0_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  EXPECT_CALL(*partition_publisher_1_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  auto shutdown = multipartition_publisher_->Shutdown();

  // shouldn't be satisfied until outstanding num partitions req is satisfied
  EXPECT_EQ(shutdown.wait_for(std::chrono::seconds(2)),
            std::future_status::timeout);
  num_partitions_1.set_value(ExamplePartitionsResponse(3));

  partition_publisher_start_0.set_value(Status());
  partition_publisher_start_1.set_value(Status());

  EXPECT_EQ(start.get(), Status());

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(partition_publisher_0_.get()));
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(partition_publisher_1_.get()));
}

TEST_F(MultipartitionPublisherTest, PublishBeforePublisherCreatedGood) {
  InSequence seq;

  promise<StatusOr<TopicPartitions>> num_partitions;
  EXPECT_CALL(*admin_connection_,
              AsyncGetTopicPartitions(IsProtoEqual(ExamplePartitionsRequest())))
      .WillOnce(Return(ByMove(num_partitions.get_future())));

  auto start = multipartition_publisher_->Start();

  PubSubMessage m0;
  *m0.mutable_data() = "data0";
  future<StatusOr<MessageMetadata>> message0 =
      multipartition_publisher_->Publish(m0);
  PubSubMessage m1;
  *m1.mutable_data() = "data1";
  future<StatusOr<MessageMetadata>> message1 =
      multipartition_publisher_->Publish(m1);

  EXPECT_CALL(partition_publisher_factory_, Call(0))
      .WillOnce(Return(partition_publisher_0_));
  promise<Status> partition_publisher_start_0;
  EXPECT_CALL(*partition_publisher_0_, Start)
      .WillOnce(Return(ByMove(partition_publisher_start_0.get_future())));
  EXPECT_CALL(partition_publisher_factory_, Call(1))
      .WillOnce(Return(partition_publisher_1_));
  promise<Status> partition_publisher_start_1;
  EXPECT_CALL(*partition_publisher_1_, Start)
      .WillOnce(Return(ByMove(partition_publisher_start_1.get_future())));

  EXPECT_CALL(routing_policy_, Route(2)).WillOnce(Return(1));
  promise<StatusOr<Cursor>> m0_promise;
  EXPECT_CALL(*partition_publisher_1_, Publish(IsProtoEqual(m0)))
      .WillOnce(Return(ByMove(m0_promise.get_future())));

  EXPECT_CALL(routing_policy_, Route(2)).WillOnce(Return(0));
  promise<StatusOr<Cursor>> m1_promise;
  EXPECT_CALL(*partition_publisher_0_, Publish(IsProtoEqual(m1)))
      .WillOnce(Return(ByMove(m1_promise.get_future())));

  num_partitions.set_value(ExamplePartitionsResponse(2));

  Cursor m0_cursor;
  m0_cursor.set_offset(kOffsetPlaceholder);
  Cursor m1_cursor;
  m1_cursor.set_offset(kOffsetPlaceholder);

  m0_promise.set_value(m0_cursor);
  m1_promise.set_value(m1_cursor);

  EXPECT_EQ(*message0.get(), (MessageMetadata{1, m0_cursor}));
  EXPECT_EQ(*message1.get(), (MessageMetadata{0, m1_cursor}));

  EXPECT_CALL(alarm_token_, Destroy);
  EXPECT_CALL(*partition_publisher_0_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  EXPECT_CALL(*partition_publisher_1_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  multipartition_publisher_->Shutdown();

  partition_publisher_start_0.set_value(Status());
  partition_publisher_start_1.set_value(Status());

  EXPECT_EQ(start.get(), Status());

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(partition_publisher_0_.get()));
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(partition_publisher_1_.get()));
}

class InitializedMultipartitionPublisherTest
    : public MultipartitionPublisherTest {
 protected:
  InitializedMultipartitionPublisherTest() {
    InSequence seq;

    promise<StatusOr<TopicPartitions>> num_partitions;
    EXPECT_CALL(
        *admin_connection_,
        AsyncGetTopicPartitions(IsProtoEqual(ExamplePartitionsRequest())))
        .WillOnce(Return(ByMove(num_partitions.get_future())));

    start_ = multipartition_publisher_->Start();

    EXPECT_CALL(partition_publisher_factory_, Call(0))
        .WillOnce(Return(partition_publisher_0_));
    EXPECT_CALL(*partition_publisher_0_, Start)
        .WillOnce(Return(ByMove(partition_publisher_start_0_.get_future())));
    EXPECT_CALL(partition_publisher_factory_, Call(1))
        .WillOnce(Return(partition_publisher_1_));
    EXPECT_CALL(*partition_publisher_1_, Start)
        .WillOnce(Return(ByMove(partition_publisher_start_1_.get_future())));

    num_partitions.set_value(ExamplePartitionsResponse(2));
  }

  ~InitializedMultipartitionPublisherTest() override {
    InSequence seq;

    EXPECT_CALL(alarm_token_, Destroy);
    EXPECT_CALL(*partition_publisher_0_, Shutdown)
        .WillOnce(Return(ByMove(make_ready_future())));
    EXPECT_CALL(*partition_publisher_1_, Shutdown)
        .WillOnce(Return(ByMove(make_ready_future())));

    multipartition_publisher_->Shutdown();

    partition_publisher_start_0_.set_value(Status());
    partition_publisher_start_1_.set_value(Status());

    EXPECT_EQ(start_.get(), Status());

    EXPECT_TRUE(Mock::VerifyAndClearExpectations(partition_publisher_0_.get()));
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(partition_publisher_1_.get()));
  }

  future<Status> start_;
  promise<Status> partition_publisher_start_0_;
  promise<Status> partition_publisher_start_1_;
};

TEST_F(InitializedMultipartitionPublisherTest, InitializesNewPartitions) {
  InSequence seq;

  promise<StatusOr<TopicPartitions>> num_partitions;
  EXPECT_CALL(*admin_connection_,
              AsyncGetTopicPartitions(IsProtoEqual(ExamplePartitionsRequest())))
      .WillOnce(Return(ByMove(num_partitions.get_future())));
  on_alarm_();

  auto partition_publisher_2 =
      absl::make_unique<StrictMock<MockPartitionPublisher>>();
  auto& partition_publisher_2_ref = *partition_publisher_2;
  EXPECT_CALL(partition_publisher_factory_, Call(2))
      .WillOnce(Return(ByMove(std::move(partition_publisher_2))));
  promise<Status> partition_publisher_start;
  EXPECT_CALL(partition_publisher_2_ref, Start)
      .WillOnce(Return(ByMove(partition_publisher_start.get_future())));
  num_partitions.set_value(ExamplePartitionsResponse(3));

  PubSubMessage m;
  *m.mutable_key() = "key";
  *m.mutable_data() = "data3";
  EXPECT_CALL(routing_policy_, Route(m.key(), 3)).WillOnce(Return(2));
  Cursor cursor;
  cursor.set_offset(kOffsetPlaceholder);
  EXPECT_CALL(partition_publisher_2_ref, Publish(IsProtoEqual(m)))
      .WillOnce(Return(ByMove(make_ready_future(make_status_or(cursor)))));
  future<StatusOr<MessageMetadata>> message =
      multipartition_publisher_->Publish(m);
  EXPECT_EQ(*message.get(), (MessageMetadata{2, cursor}));

  EXPECT_CALL(partition_publisher_2_ref, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  partition_publisher_start.set_value(Status());
}

TEST_F(InitializedMultipartitionPublisherTest, GetNumPartitionsSame) {
  EXPECT_CALL(*admin_connection_,
              AsyncGetTopicPartitions(IsProtoEqual(ExamplePartitionsRequest())))
      .WillOnce(Return(ByMove(ReadyTopicPartitionsFuture(2))));
  on_alarm_();
}

TEST_F(InitializedMultipartitionPublisherTest, InitializesNewPartitionsFails) {
  EXPECT_CALL(*admin_connection_,
              AsyncGetTopicPartitions(IsProtoEqual(ExamplePartitionsRequest())))
      .WillOnce(
          Return(ByMove(ReadyTopicPartitionsFuture(kOutOfBoundsPartition))));
  on_alarm_();
  // everything finishes validly as only second poll failed
}

TEST_F(InitializedMultipartitionPublisherTest, Flush) {
  InSequence seq;

  EXPECT_CALL(*partition_publisher_0_, Flush);
  EXPECT_CALL(*partition_publisher_1_, Flush);
  multipartition_publisher_->Flush();
}

TEST_F(InitializedMultipartitionPublisherTest, RegularPublishes) {
  InSequence seq;

  PubSubMessage m1;
  *m1.mutable_key() = "key";
  *m1.mutable_data() = "data";
  EXPECT_CALL(routing_policy_, Route(m1.key(), 2)).WillOnce(Return(1));
  Cursor m1_cursor;
  m1_cursor.set_offset(kOffsetPlaceholder);
  EXPECT_CALL(*partition_publisher_1_, Publish(IsProtoEqual(m1)))
      .WillOnce(Return(ByMove(make_ready_future(make_status_or(m1_cursor)))));
  future<StatusOr<MessageMetadata>> message1 =
      multipartition_publisher_->Publish(m1);
  EXPECT_EQ(*message1.get(), (MessageMetadata{1, m1_cursor}));

  PubSubMessage m2;
  *m2.mutable_key() = "key";
  *m2.mutable_data() = "data1";
  EXPECT_CALL(routing_policy_, Route(m2.key(), 2)).WillOnce(Return(0));
  Cursor m2_cursor;
  m2_cursor.set_offset(kOffsetPlaceholder);
  EXPECT_CALL(*partition_publisher_0_, Publish(IsProtoEqual(m2)))
      .WillOnce(Return(ByMove(make_ready_future(make_status_or(m2_cursor)))));
  future<StatusOr<MessageMetadata>> message2 =
      multipartition_publisher_->Publish(m2);
  EXPECT_EQ(*message2.get(), (MessageMetadata{0, m2_cursor}));
}

TEST_F(InitializedMultipartitionPublisherTest, PublishFail) {
  InSequence seq;

  PubSubMessage m;
  *m.mutable_key() = "key";
  *m.mutable_data() = "data";
  EXPECT_CALL(routing_policy_, Route(m.key(), 2)).WillOnce(Return(0));
  EXPECT_CALL(*partition_publisher_0_, Publish(IsProtoEqual(m)))
      .WillOnce(Return(ByMove(make_ready_future(
          StatusOr<Cursor>(Status(StatusCode::kAborted, "abort abort"))))));
  future<StatusOr<MessageMetadata>> message =
      multipartition_publisher_->Publish(m);
  EXPECT_EQ(message.get().status(),
            Status(StatusCode::kAborted, "abort abort"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google
