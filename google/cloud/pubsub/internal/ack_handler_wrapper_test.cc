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

#include "google/cloud/pubsub/internal/ack_handler_wrapper.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::internal::PermissionDeniedError;
using ::google::cloud::testing_util::ScopedLog;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;

class MockExactlyOnceAckHandlerImpl
    : public pubsub::ExactlyOnceAckHandler::Impl {
 public:
  MOCK_METHOD(future<Status>, ack, (), (override));
  MOCK_METHOD(future<Status>, nack, (), (override));
  MOCK_METHOD(std::int32_t, delivery_attempt, (), (const, override));
};

TEST(AckHandlerWrapper, Ack) {
  auto mock = absl::make_unique<MockExactlyOnceAckHandlerImpl>();
  EXPECT_CALL(*mock, ack)
      .WillOnce(
          Return(ByMove(make_ready_future(PermissionDeniedError("uh-oh")))));
  ScopedLog log;
  AckHandlerWrapper tested(std::move(mock), "test-id");
  tested.ack();
  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr(" ack()"), HasSubstr("uh-oh"),
                             HasSubstr("test-id"))));
}

TEST(AckHandlerWrapper, AckEmpty) {
  auto mock = absl::make_unique<MockExactlyOnceAckHandlerImpl>();
  EXPECT_CALL(*mock, ack)
      .WillOnce(
          Return(ByMove(make_ready_future(PermissionDeniedError("uh-oh")))));
  ScopedLog log;
  AckHandlerWrapper tested(std::move(mock));
  tested.ack();
  EXPECT_THAT(log.ExtractLines(), Not(Contains(HasSubstr(" ack()"))));
}

TEST(AckHandlerWrapper, Nack) {
  auto mock = absl::make_unique<MockExactlyOnceAckHandlerImpl>();
  EXPECT_CALL(*mock, nack)
      .WillOnce(
          Return(ByMove(make_ready_future(PermissionDeniedError("uh-oh")))));
  ScopedLog log;
  AckHandlerWrapper tested(std::move(mock), "test-id");
  tested.nack();
  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr(" nack()"), HasSubstr("uh-oh"),
                             HasSubstr("test-id"))));
}

TEST(AckHandlerWrapper, NackEmpty) {
  auto mock = absl::make_unique<MockExactlyOnceAckHandlerImpl>();
  EXPECT_CALL(*mock, nack)
      .WillOnce(
          Return(ByMove(make_ready_future(PermissionDeniedError("uh-oh")))));
  ScopedLog log;
  AckHandlerWrapper tested(std::move(mock));
  tested.nack();
  EXPECT_THAT(log.ExtractLines(), Not(Contains(HasSubstr(" nack()"))));
}

TEST(AckHandlerWrapper, DeliveryAttempt) {
  auto mock = absl::make_unique<MockExactlyOnceAckHandlerImpl>();
  EXPECT_CALL(*mock, delivery_attempt).WillOnce(Return(42));
  AckHandlerWrapper tested(std::move(mock), "test-id");
  EXPECT_EQ(tested.delivery_attempt(), 42);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
