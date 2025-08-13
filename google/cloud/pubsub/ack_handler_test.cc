// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/ack_handler.h"
#include "google/cloud/pubsub/mocks/mock_ack_handler.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Return;

TEST(AckHandlerTest, AutoNack) {
  auto mock = std::make_unique<pubsub_mocks::MockAckHandler>();
  EXPECT_CALL(*mock, nack()).Times(1);
  {
    AckHandler handler(std::move(mock));
  }
}

TEST(AckHandlerTest, AutoNackMove) {
  auto mock = std::make_unique<pubsub_mocks::MockAckHandler>();
  EXPECT_CALL(*mock, ack()).Times(1);
  {
    AckHandler handler(std::move(mock));
    AckHandler moved = std::move(handler);
    std::move(moved).ack();
  }
}

TEST(AckHandlerTest, DeliveryAttempts) {
  auto mock = std::make_unique<pubsub_mocks::MockAckHandler>();
  EXPECT_CALL(*mock, delivery_attempt()).WillOnce(Return(42));
  EXPECT_CALL(*mock, nack()).Times(1);
  AckHandler handler(std::move(mock));
  EXPECT_EQ(42, handler.delivery_attempt());
}

TEST(AckHandlerTest, Ack) {
  auto mock = std::make_unique<pubsub_mocks::MockAckHandler>();
  EXPECT_CALL(*mock, ack()).Times(1);
  AckHandler handler(std::move(mock));
  ASSERT_NO_FATAL_FAILURE(std::move(handler).ack());
}

TEST(AckHandlerTest, Nack) {
  auto mock = std::make_unique<pubsub_mocks::MockAckHandler>();
  EXPECT_CALL(*mock, nack()).Times(1);
  AckHandler handler(std::move(mock));
  ASSERT_NO_FATAL_FAILURE(std::move(handler).nack());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
