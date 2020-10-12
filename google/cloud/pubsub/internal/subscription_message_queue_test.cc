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

#include "google/cloud/pubsub/internal/subscription_message_queue.h"
#include "google/cloud/pubsub/testing/mock_subscription_batch_source.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::ElementsAre;

std::vector<google::pubsub::v1::ReceivedMessage> GenerateMessages() {
  auto constexpr kTextM0 = R"pb(
    message {
      data: "m0"
      attributes: { key: "k0" value: "m0-l0" }
      attributes: { key: "k1" value: "m0-l1" }
      message_id: "id-m0"
      ordering_key: "abcd"
    }
    ack_id: "ack-m0"
  )pb";
  auto constexpr kTextM1 = R"pb(
    message {
      data: "m1"
      attributes: { key: "k0" value: "m1-l0" }
      attributes: { key: "k1" value: "m1-l1" }
      message_id: "id-m1"
      ordering_key: "abcd"
    }
    ack_id: "ack-m1"
  )pb";
  auto constexpr kTextM2 = R"pb(
    message {
      data: "m2"
      attributes: { key: "k0" value: "m2-l0" }
      attributes: { key: "k1" value: "m2-l1" }
      message_id: "id-m2"
      ordering_key: "abcd"
    }
    ack_id: "ack-m2"
  )pb";
  google::pubsub::v1::ReceivedMessage m0;
  if (!google::protobuf::TextFormat::ParseFromString(kTextM0, &m0)) return {};
  google::pubsub::v1::ReceivedMessage m1;
  if (!google::protobuf::TextFormat::ParseFromString(kTextM1, &m1)) return {};
  google::pubsub::v1::ReceivedMessage m2;
  if (!google::protobuf::TextFormat::ParseFromString(kTextM2, &m2)) return {};

  return {m0, m1, m2};
}

google::pubsub::v1::StreamingPullResponse GenerateResponse(
    std::vector<google::pubsub::v1::ReceivedMessage> messages) {
  google::pubsub::v1::StreamingPullResponse response;
  for (auto& m : messages) {
    *response.add_received_messages() = std::move(m);
  }
  return response;
}

TEST(SubscriptionMessageQueueTest, Basic) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();
  EXPECT_CALL(*mock, Shutdown).Times(1);
  BatchCallback batch_callback;
  EXPECT_CALL(*mock, Start).WillOnce([&](BatchCallback cb) {
    batch_callback = std::move(cb);
  });

  EXPECT_CALL(*mock, AckMessage)
      .WillOnce([](std::string const& ack_id, std::size_t size) {
        EXPECT_EQ("ack-m0", ack_id);
        EXPECT_EQ(0, size);
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, NackMessage)
      .WillOnce([](std::string const& ack_id, std::size_t size) {
        EXPECT_EQ("ack-m1", ack_id);
        EXPECT_EQ(1, size);
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, BulkNack).Times(1);

  auto const messages = GenerateMessages();
  ASSERT_FALSE(messages.empty());
  auto const response = GenerateResponse(messages);

  std::vector<google::pubsub::v1::ReceivedMessage> received;
  auto handler = [&received](google::pubsub::v1::ReceivedMessage m) {
    received.push_back(std::move(m));
  };

  auto shutdown = std::make_shared<SessionShutdownManager>();
  shutdown->Start({});
  auto uut = SubscriptionMessageQueue::Create(shutdown, mock);
  uut->Start(handler);

  uut->Read(1);
  EXPECT_THAT(received, ElementsAre());

  batch_callback(response);
  uut->Read(1);

  EXPECT_THAT(received, ElementsAre(IsProtoEqual(messages[0]),
                                    IsProtoEqual(messages[1])));
  uut->AckMessage("ack-m0", 0);
  uut->NackMessage("ack-m1", 1);

  received.clear();
  uut->Shutdown();
}

/// @test Verify that messages received after a shutdown are nacked.
TEST(SubscriptionMessageQueueTest, NackOnSessionShutdown) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();
  EXPECT_CALL(*mock, Shutdown);
  BatchCallback batch_callback;
  EXPECT_CALL(*mock, Start).WillOnce([&](BatchCallback cb) {
    batch_callback = std::move(cb);
  });

  EXPECT_CALL(*mock, BulkNack)
      .WillOnce([&](std::vector<std::string> const& ack_ids, std::size_t) {
        EXPECT_THAT(ack_ids, ElementsAre("ack-m0", "ack-m1", "ack-m2"));
        return make_ready_future(Status{});
      });

  ::testing::MockFunction<void(google::pubsub::v1::ReceivedMessage const&)>
      mock_handler;

  auto const response = GenerateResponse(GenerateMessages());
  EXPECT_FALSE(response.received_messages().empty());

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = SubscriptionMessageQueue::Create(shutdown, mock);
  uut->Start(mock_handler.AsStdFunction());
  uut->Read(1);
  shutdown->MarkAsShutdown("test", {});
  batch_callback(response);
  uut->Shutdown();
}

/// @test Verify that receiving an error triggers the right shutdown.
TEST(SubscriptionMessageQueueTest, HandleError) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();
  BatchCallback batch_callback;
  EXPECT_CALL(*mock, Start).WillOnce([&](BatchCallback cb) {
    batch_callback = std::move(cb);
  });

  ::testing::MockFunction<void(google::pubsub::v1::ReceivedMessage const&)>
      mock_handler;

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = SubscriptionMessageQueue::Create(shutdown, mock);
  auto done = shutdown->Start({});
  uut->Start(mock_handler.AsStdFunction());
  uut->Read(1);
  auto const expected = Status{StatusCode::kPermissionDenied, "uh-oh"};
  batch_callback(expected);

  EXPECT_EQ(done.get(), expected);
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
