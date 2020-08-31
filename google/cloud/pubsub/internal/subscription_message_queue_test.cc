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

TEST(SubscriptionMessageQueueTest, Basic) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();
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
  SubscriptionMessageQueue uut(mock);

  std::vector<google::pubsub::v1::ReceivedMessage> received;
  auto handler = [&received](google::pubsub::v1::ReceivedMessage m) {
    received.push_back(std::move(m));
  };

  uut.Start(handler);
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
  google::pubsub::v1::ReceivedMessage m1;
  google::pubsub::v1::ReceivedMessage m2;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kTextM0, &m0));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kTextM1, &m1));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kTextM2, &m2));

  auto const text_response = [&] {
    std::ostringstream os;
    os << "received_messages { " << kTextM0 << "}";
    os << "received_messages { " << kTextM1 << "}";
    os << "received_messages { " << kTextM2 << "}";
    return std::move(os).str();
  }();
  google::pubsub::v1::PullResponse response;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(text_response, &response));

  uut.Read(1);
  EXPECT_THAT(received, ElementsAre());

  uut.OnPull(response);
  uut.Read(1);

  EXPECT_THAT(received, ElementsAre(IsProtoEqual(m0), IsProtoEqual(m1)));
  uut.AckMessage("ack-m0", 0);
  uut.NackMessage("ack-m1", 1);

  received.clear();
  uut.Shutdown();
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
