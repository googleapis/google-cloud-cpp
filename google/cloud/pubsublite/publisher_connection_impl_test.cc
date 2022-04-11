// Copyright 2022 Google LLC
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

#include "google/cloud/pubsublite/publisher_connection_impl.h"
#include "google/cloud/pubsublite/testing/mock_publisher.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::pubsub::Message;
using ::google::cloud::pubsublite::MessageMetadata;
using ::google::cloud::pubsublite_testing::MockPublisher;
using ::google::cloud::testing_util::IsProtoEqual;
using PublishParams =
    ::google::cloud::pubsub::PublisherConnection::PublishParams;
using FlushParams = ::google::cloud::pubsub::PublisherConnection::FlushParams;
using ::google::cloud::pubsub_internal::FromProto;

using ::google::cloud::pubsublite::v1::Cursor;
using ::google::cloud::pubsublite::v1::PubSubMessage;
using ::google::pubsub::v1::PubsubMessage;

using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;

TEST(PublisherConnectionImplTest, BadMessage) {
  InSequence seq;

  auto publisher =
      absl::make_unique<StrictMock<MockPublisher<MessageMetadata>>>();
  StrictMock<MockPublisher<MessageMetadata>>& publisher_ref = *publisher;
  Status status = Status{StatusCode::kAborted, "uh ohhh"};
  StrictMock<MockFunction<StatusOr<PubSubMessage>(Message)>> transformer;
  promise<Status> p;
  EXPECT_CALL(publisher_ref, Start).WillOnce(Return(ByMove(p.get_future())));
  PublisherConnectionImpl conn{std::move(publisher),
                               transformer.AsStdFunction()};

  EXPECT_CALL(transformer, Call).WillOnce(Return(status));
  auto received = conn.Publish(PublishParams{FromProto(PubsubMessage{})});
  auto error = received.get().status();
  EXPECT_EQ(error, status);

  EXPECT_CALL(publisher_ref, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
}

TEST(PublisherConnectionImplTest, GoodMessageBadPublish) {
  InSequence seq;

  auto publisher =
      absl::make_unique<StrictMock<MockPublisher<MessageMetadata>>>();
  StrictMock<MockPublisher<MessageMetadata>>& publisher_ref = *publisher;
  StrictMock<MockFunction<StatusOr<PubSubMessage>(Message)>> transformer;
  promise<Status> p;
  EXPECT_CALL(publisher_ref, Start).WillOnce(Return(ByMove(p.get_future())));
  PublisherConnectionImpl conn{std::move(publisher),
                               transformer.AsStdFunction()};

  PubSubMessage message;
  message.set_key("1");
  *message.mutable_data() = "dataaaa";
  EXPECT_CALL(transformer, Call).WillOnce(Return(message));
  Status status = Status(StatusCode::kUnavailable, "booked");
  EXPECT_CALL(publisher_ref, Publish(IsProtoEqual(message)))
      .WillOnce(
          Return(ByMove(make_ready_future(StatusOr<MessageMetadata>(status)))));
  auto received = conn.Publish(PublishParams{FromProto(PubsubMessage{})});
  auto error = received.get().status();
  EXPECT_EQ(error, status);

  EXPECT_CALL(publisher_ref, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
}

TEST(PublisherConnectionImplTest, GoodMessageGoodPublish) {
  InSequence seq;

  auto publisher =
      absl::make_unique<StrictMock<MockPublisher<MessageMetadata>>>();
  StrictMock<MockPublisher<MessageMetadata>>& publisher_ref = *publisher;
  StrictMock<MockFunction<StatusOr<PubSubMessage>(Message)>> transformer;
  promise<Status> p;
  EXPECT_CALL(publisher_ref, Start).WillOnce(Return(ByMove(p.get_future())));
  PublisherConnectionImpl conn{std::move(publisher),
                               transformer.AsStdFunction()};

  PubSubMessage message;
  message.set_key("2");
  *message.mutable_data() = "hello";
  EXPECT_CALL(transformer, Call).WillOnce(Return(message));
  MessageMetadata mm{42, Cursor{}};
  EXPECT_CALL(publisher_ref, Publish(IsProtoEqual(message)))
      .WillOnce(
          Return(ByMove(make_ready_future(StatusOr<MessageMetadata>(mm)))));
  auto received = conn.Publish(PublishParams{FromProto(PubsubMessage{})}).get();
  EXPECT_EQ(*received, mm.Serialize());

  EXPECT_CALL(publisher_ref, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
}

TEST(PublisherConnectionImplTest, Flush) {
  InSequence seq;

  auto publisher =
      absl::make_unique<StrictMock<MockPublisher<MessageMetadata>>>();
  StrictMock<MockFunction<StatusOr<PubSubMessage>(Message)>> transformer;
  StrictMock<MockPublisher<MessageMetadata>>& publisher_ref = *publisher;
  promise<Status> p;
  EXPECT_CALL(publisher_ref, Start).WillOnce(Return(ByMove(p.get_future())));
  PublisherConnectionImpl conn{std::move(publisher),
                               transformer.AsStdFunction()};

  EXPECT_CALL(publisher_ref, Flush);
  conn.Flush(FlushParams{});

  EXPECT_CALL(publisher_ref, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google
