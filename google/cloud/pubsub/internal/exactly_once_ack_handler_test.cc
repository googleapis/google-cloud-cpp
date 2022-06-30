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

#include "google/cloud/pubsub/internal/exactly_once_ack_handler.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::ByMove;
using ::testing::Return;

// TODO(#9327) move this class to the public mocks
/**
 * A googlemock-based mock for
 * [pubsub::ExactlyOnceAckHandler::Impl][mocked-link]
 *
 * [mocked-link]: @ref
 * google::cloud::pubsub_internal::ExactlyOnceAckHandler::Impl
 *
 * @see @ref subscriber-exactly-once-mock for an example using this class.
 */
class MockExactlyOnceAckHandler : public ExactlyOnceAckHandler::Impl {
 public:
  MOCK_METHOD(future<Status>, ack, (), (override));
  MOCK_METHOD(future<Status>, nack, (), (override));
  MOCK_METHOD(std::string, ack_id, ());
  MOCK_METHOD(std::int32_t, delivery_attempt, (), (const, override));
};

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
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
