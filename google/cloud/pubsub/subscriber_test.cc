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

#include "google/cloud/pubsub/subscriber.h"
#include "google/cloud/pubsub/mocks/mock_ack_handler.h"
#include "google/cloud/pubsub/mocks/mock_subscriber_connection.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::testing::_;

/// @test Verify Subscriber::Subscribe() works, including mocks.
TEST(SubscriberTest, SubscribeSimple) {
  Subscription const subscription("test-project", "test-subscription");
  auto mock = std::make_shared<pubsub_mocks::MockSubscriberConnection>();
  EXPECT_CALL(*mock, Subscribe(_))
      .WillOnce([&](SubscriberConnection::SubscribeParams const& p) {
        EXPECT_EQ(subscription.FullName(), p.full_subscription_name);

        {
          auto ack = absl::make_unique<pubsub_mocks::MockAckHandler>();
          EXPECT_CALL(*ack, ack()).Times(1);
          p.callback(pubsub::MessageBuilder{}.SetData("do-ack").Build(),
                     AckHandler(std::move(ack)));
        }

        {
          auto ack = absl::make_unique<pubsub_mocks::MockAckHandler>();
          EXPECT_CALL(*ack, nack()).Times(1);
          p.callback(pubsub::MessageBuilder{}.SetData("do-nack").Build(),
                     AckHandler(std::move(ack)));
        }

        return make_ready_future(Status{});
      });

  Subscriber subscriber(mock);
  auto status = subscriber
                    .Subscribe(subscription,
                               [&](Message const& m, AckHandler h) {
                                 if (m.data() == "do-nack") {
                                   std::move(h).nack();
                                 } else {
                                   std::move(h).ack();
                                 }
                               })
                    .get();
  ASSERT_STATUS_OK(status);
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
