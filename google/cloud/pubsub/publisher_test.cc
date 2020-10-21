// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/publisher.h"
#include "google/cloud/pubsub/mocks/mock_publisher_connection.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Return;

TEST(PublisherTest, PublishSimple) {
  auto const topic = pubsub::Topic("test-project", "test-topic");
  auto const message = pubsub::MessageBuilder{}.SetData("test-data-0").Build();

  auto mock = std::make_shared<pubsub_mocks::MockPublisherConnection>();
  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  EXPECT_CALL(*mock, cq).WillRepeatedly(Return(background.cq()));
  EXPECT_CALL(*mock, Publish)
      .WillOnce([&](PublisherConnection::PublishParams const& p) {
        EXPECT_EQ(p.request.topic(), topic.FullName());
        EXPECT_THAT(
            p.request.messages(),
            ElementsAre(IsProtoEqual(pubsub_internal::ToProto(message))));
        google::pubsub::v1::PublishResponse response;
        response.add_message_ids("test-id-0");
        return make_ready_future(make_status_or(response));
      });

  Publisher publisher(topic, mock);
  publisher.Flush();
  auto id = publisher.Publish(message).get();
  ASSERT_STATUS_OK(id);
  EXPECT_EQ("test-id-0", *id);
}

TEST(PublisherTest, OrderingKey) {
  auto const topic = Topic("test-project", "test-topic");
  auto const message = MessageBuilder{}.SetData("test-data-0").Build();

  auto mock = std::make_shared<pubsub_mocks::MockPublisherConnection>();
  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  EXPECT_CALL(*mock, cq).WillRepeatedly(Return(background.cq()));
  EXPECT_CALL(*mock, Publish)
      .WillOnce([&](PublisherConnection::PublishParams const& p) {
        EXPECT_EQ(topic.FullName(), p.request.topic());
        EXPECT_THAT(
            p.request.messages(),
            ElementsAre(IsProtoEqual(pubsub_internal::ToProto(message))));
        google::pubsub::v1::PublishResponse response;
        response.add_message_ids("test-message-id-0");
        return make_ready_future(make_status_or(response));
      });

  auto publisher =
      Publisher(topic, PublisherOptions{}.enable_message_ordering(), mock);
  auto response = publisher.Publish(message).get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ("test-message-id-0", *response);
}

TEST(PublisherTest, OrderingKeyWithoutMessageOrdering) {
  auto const topic = Topic("test-project", "test-topic");

  auto mock = std::make_shared<pubsub_mocks::MockPublisherConnection>();
  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  EXPECT_CALL(*mock, cq).WillRepeatedly(Return(background.cq()));
  EXPECT_CALL(*mock, Publish).Times(0);

  auto publisher = Publisher(topic, PublisherOptions{}, mock);
  auto response = publisher
                      .Publish({MessageBuilder{}
                                    .SetOrderingKey("test-ordering-key-0")
                                    .SetData("test-data-0")
                                    .Build()})
                      .get();
  EXPECT_THAT(response.status(),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("does not have message ordering enabled")));
}

TEST(PublisherTest, HandleInvalidResponse) {
  Topic const topic("test-project", "test-topic");

  auto mock = std::make_shared<pubsub_mocks::MockPublisherConnection>();
  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  EXPECT_CALL(*mock, cq).WillRepeatedly(Return(background.cq()));
  EXPECT_CALL(*mock, Publish)
      .WillOnce([&](PublisherConnection::PublishParams const&) {
        google::pubsub::v1::PublishResponse response;
        return make_ready_future(make_status_or(response));
      });

  auto publisher = Publisher(topic, PublisherOptions{}, mock);
  auto response =
      publisher.Publish({MessageBuilder{}.SetData("test-data-0").Build()})
          .get();
  // It is very unlikely we will see this in production, it would indicate a bug
  // in the Cloud Pub/Sub service where we successfully published N events, but
  // we received M != N message ids back.
  EXPECT_THAT(
      response.status(),
      StatusIs(StatusCode::kUnknown, HasSubstr("mismatched message id count")));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
