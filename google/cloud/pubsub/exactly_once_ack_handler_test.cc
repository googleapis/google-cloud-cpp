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

#include "google/cloud/pubsub/exactly_once_ack_handler.h"
#include "google/cloud/pubsub/mocks/mock_exactly_once_ack_handler.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::pubsub_mocks::MockExactlyOnceAckHandler;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ByMove;
using ::testing::Return;

TEST(AckHandlerTest, AutoNack) {
  auto mock = absl::make_unique<MockExactlyOnceAckHandler>();
  EXPECT_CALL(*mock, nack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  { ExactlyOnceAckHandler handler(std::move(mock)); }
}

TEST(AckHandlerTest, AutoNackMove) {
  auto mock = absl::make_unique<MockExactlyOnceAckHandler>();
  EXPECT_CALL(*mock, ack())
      .WillOnce(Return(ByMove(
          make_ready_future(Status{StatusCode::kPermissionDenied, "uh-oh"}))));
  {
    ExactlyOnceAckHandler handler(std::move(mock));
    ExactlyOnceAckHandler moved = std::move(handler);
    EXPECT_THAT(std::move(moved).ack().get(),
                StatusIs(StatusCode::kPermissionDenied, "uh-oh"));
  }
}

TEST(AckHandlerTest, DeliveryAttempts) {
  auto mock = absl::make_unique<MockExactlyOnceAckHandler>();
  EXPECT_CALL(*mock, delivery_attempt()).WillOnce(Return(42));
  EXPECT_CALL(*mock, nack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  ExactlyOnceAckHandler handler(std::move(mock));
  EXPECT_EQ(42, handler.delivery_attempt());
}

TEST(AckHandlerTest, Ack) {
  auto mock = absl::make_unique<MockExactlyOnceAckHandler>();
  EXPECT_CALL(*mock, ack())
      .WillOnce(Return(ByMove(
          make_ready_future(Status{StatusCode::kPermissionDenied, "uh-oh"}))));
  ExactlyOnceAckHandler handler(std::move(mock));
  EXPECT_THAT(std::move(handler).ack().get(),
              StatusIs(StatusCode::kPermissionDenied, "uh-oh"));
}

TEST(AckHandlerTest, Nack) {
  auto mock = absl::make_unique<MockExactlyOnceAckHandler>();
  EXPECT_CALL(*mock, nack())
      .WillOnce(Return(ByMove(
          make_ready_future(Status{StatusCode::kPermissionDenied, "uh-oh"}))));
  ExactlyOnceAckHandler handler(std::move(mock));
  EXPECT_THAT(std::move(handler).nack().get(),
              StatusIs(StatusCode::kPermissionDenied, "uh-oh"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
