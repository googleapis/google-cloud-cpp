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

#include "google/cloud/pubsublite/internal/publisher_connection_impl.h"
#include "google/cloud/pubsublite/testing/mock_publisher.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsublite_internal {
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

class PublisherConnectionImplTest : public ::testing::Test {
 protected:
  PublisherConnectionImplTest()
      : publisher_ref_{*new StrictMock<MockPublisher<MessageMetadata>>()} {
    EXPECT_CALL(publisher_ref_, Start)
        .WillOnce(Return(ByMove(status_promise_.get_future())));
    conn_ = absl::make_unique<PublisherConnectionImpl>(
        absl::WrapUnique(&publisher_ref_), transformer_.AsStdFunction());
  }

  ~PublisherConnectionImplTest() override {
    EXPECT_CALL(publisher_ref_, Shutdown)
        .WillOnce(Return(ByMove(make_ready_future())));
  }

  promise<Status> status_promise_;
  StrictMock<MockPublisher<MessageMetadata>>& publisher_ref_;
  StrictMock<MockFunction<StatusOr<PubSubMessage>(Message)>> transformer_;
  std::unique_ptr<google::cloud::pubsub::PublisherConnection> conn_;
};

TEST_F(PublisherConnectionImplTest, BadMessage) {
  InSequence seq;
  Status status = Status{StatusCode::kAborted, "uh ohhh"};

  EXPECT_CALL(transformer_, Call).WillOnce(Return(status));
  auto received = conn_->Publish(PublishParams{FromProto(PubsubMessage{})});
  auto error = received.get().status();
  EXPECT_EQ(error, status);
}

TEST_F(PublisherConnectionImplTest, GoodMessageBadPublish) {
  InSequence seq;

  PubSubMessage message;
  message.set_key("1");
  *message.mutable_data() = "dataaaa";
  EXPECT_CALL(transformer_, Call).WillOnce(Return(message));
  Status status = Status(StatusCode::kUnavailable, "booked");
  EXPECT_CALL(publisher_ref_, Publish(IsProtoEqual(message)))
      .WillOnce(
          Return(ByMove(make_ready_future(StatusOr<MessageMetadata>(status)))));
  auto received = conn_->Publish(PublishParams{FromProto(PubsubMessage{})});
  auto error = received.get().status();
  EXPECT_EQ(error, status);
}

TEST_F(PublisherConnectionImplTest, GoodMessageGoodPublish) {
  InSequence seq;

  PubSubMessage message;
  message.set_key("2");
  *message.mutable_data() = "hello";
  EXPECT_CALL(transformer_, Call).WillOnce(Return(message));
  MessageMetadata mm{42, Cursor{}};
  EXPECT_CALL(publisher_ref_, Publish(IsProtoEqual(message)))
      .WillOnce(
          Return(ByMove(make_ready_future(StatusOr<MessageMetadata>(mm)))));
  auto received =
      conn_->Publish(PublishParams{FromProto(PubsubMessage{})}).get();
  EXPECT_EQ(*received, mm.Serialize());
}

TEST_F(PublisherConnectionImplTest, Flush) {
  InSequence seq;

  EXPECT_CALL(publisher_ref_, Flush);
  conn_->Flush(FlushParams{});
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google
