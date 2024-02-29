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

#include "google/cloud/pubsub/internal/default_pull_ack_handler.h"
#include "google/cloud/pubsub/internal/default_pull_lease_manager.h"
#include "google/cloud/pubsub/retry_policy.h"
#include "google/cloud/pubsub/testing/mock_pull_lease_manager.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::pubsub_testing::MockPullLeaseManager;
using ::google::cloud::pubsub_testing::MockSubscriberStub;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::StatusIs;
using ::google::pubsub::v1::AcknowledgeRequest;
using ::google::pubsub::v1::ModifyAckDeadlineRequest;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Property;
using ::testing::Return;

using MockClock =
    ::testing::MockFunction<std::chrono::system_clock::time_point()>;

CompletionQueue MakeMockCompletionQueue(AsyncSequencer<bool>& aseq) {
  auto mock = std::make_shared<testing_util::MockCompletionQueueImpl>();
  EXPECT_CALL(*mock, MakeRelativeTimer).WillRepeatedly([&](auto) {
    return aseq.PushBack("MakeRelativeTimer")
        .then([](auto f) -> StatusOr<std::chrono::system_clock::time_point> {
          if (!f.get()) return Status(StatusCode::kCancelled, "timer");
          return std::chrono::system_clock::now();
        });
  });
  return CompletionQueue(std::move(mock));
}

Status TransientError() {
  return Status(StatusCode::kUnavailable, "try-again");
}

Status TransientError(std::string ack_id) {
  return Status(StatusCode::kUnknown, "oh noes!",
                ErrorInfo("test-only-reason", "test-only-domain",
                          {
                              {"some-other-id", "PERMANENT_"},
                              {ack_id, "TRANSIENT_FAILURE_NO_BIGGIE"},
                          }));
}

Status PermanentError() {
  return Status(StatusCode::kPermissionDenied, "uh-oh");
}

TEST(PullAckHandlerTest, AckSimple) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");

  auto mock = std::make_shared<MockSubscriberStub>();
  auto request_matcher = AllOf(
      Property(&AcknowledgeRequest::ack_ids, ElementsAre("test-ack-id")),
      Property(&AcknowledgeRequest::subscription, subscription.FullName()));
  EXPECT_CALL(*mock, AsyncAcknowledge(_, _, _, request_matcher))
      .WillOnce(Return(ByMove(make_ready_future(TransientError()))))
      .WillOnce(
          Return(ByMove(make_ready_future(TransientError("test-ack-id")))))
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto handler = std::make_unique<DefaultPullAckHandler>(
      cq, mock, subscription, "test-ack-id", 42,
      std::make_shared<MockPullLeaseManager>());
  EXPECT_EQ(handler->delivery_attempt(), 42);
  auto status = handler->ack();
  auto timer = aseq.PopFrontWithName();
  EXPECT_EQ(timer.second, "MakeRelativeTimer");
  timer.first.set_value(true);
  timer = aseq.PopFrontWithName();
  EXPECT_EQ(timer.second, "MakeRelativeTimer");
  timer.first.set_value(true);
  EXPECT_STATUS_OK(status.get());
}

TEST(PullAckHandlerTest, AckPermanentError) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");

  auto mock = std::make_shared<MockSubscriberStub>();
  auto request_matcher = AllOf(
      Property(&AcknowledgeRequest::ack_ids, ElementsAre("test-ack-id")),
      Property(&AcknowledgeRequest::subscription, subscription.FullName()));
  EXPECT_CALL(*mock, AsyncAcknowledge(_, _, _, request_matcher))
      .WillOnce(Return(ByMove(make_ready_future(PermanentError()))));
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto handler = std::make_unique<DefaultPullAckHandler>(
      cq, mock, subscription, "test-ack-id", 42,
      std::make_shared<MockPullLeaseManager>());
  EXPECT_EQ(handler->delivery_attempt(), 42);
  auto status = handler->ack();
  EXPECT_THAT(status.get(),
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

TEST(PullAckHandlerTest, NackSimple) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");

  auto mock = std::make_shared<MockSubscriberStub>();
  auto request_matcher = AllOf(
      Property(&ModifyAckDeadlineRequest::ack_ids, ElementsAre("test-ack-id")),
      Property(&ModifyAckDeadlineRequest::ack_deadline_seconds, 0),
      Property(&ModifyAckDeadlineRequest::subscription,
               subscription.FullName()));
  EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _, _, request_matcher))
      .WillOnce(Return(ByMove(make_ready_future(TransientError()))))
      .WillOnce(
          Return(ByMove(make_ready_future(TransientError("test-ack-id")))))
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto handler = std::make_unique<DefaultPullAckHandler>(
      cq, mock, subscription, "test-ack-id", 42,
      std::make_shared<MockPullLeaseManager>());
  EXPECT_EQ(handler->delivery_attempt(), 42);
  auto status = handler->nack();
  auto timer = aseq.PopFrontWithName();
  EXPECT_EQ(timer.second, "MakeRelativeTimer");
  timer.first.set_value(true);
  timer = aseq.PopFrontWithName();
  EXPECT_EQ(timer.second, "MakeRelativeTimer");
  timer.first.set_value(true);
  EXPECT_STATUS_OK(status.get());
}

TEST(PullAckHandlerTest, NackPermanentError) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");

  auto mock = std::make_shared<MockSubscriberStub>();
  auto request_matcher = AllOf(
      Property(&ModifyAckDeadlineRequest::ack_ids, ElementsAre("test-ack-id")),
      Property(&ModifyAckDeadlineRequest::ack_deadline_seconds, 0),
      Property(&ModifyAckDeadlineRequest::subscription,
               subscription.FullName()));
  EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _, _, request_matcher))
      .WillOnce(Return(ByMove(make_ready_future(PermanentError()))));
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto handler = std::make_unique<DefaultPullAckHandler>(
      cq, mock, subscription, "test-ack-id", 42,
      std::make_shared<MockPullLeaseManager>());
  EXPECT_EQ(handler->delivery_attempt(), 42);
  auto status = handler->nack();
  EXPECT_THAT(status.get(),
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

TEST(PullAckHandlerTest, StartsLeaseManager) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");

  auto mock = std::make_shared<MockSubscriberStub>();
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto lm = std::make_shared<MockPullLeaseManager>();
  EXPECT_CALL(*lm, StartLeaseLoop()).Times(1);
  auto handler = std::make_unique<DefaultPullAckHandler>(cq, mock, subscription,
                                                         "test-ack-id", 42, lm);
}

TEST(AckHandlerTest, Subscription) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  auto mock = std::make_shared<MockSubscriberStub>();
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto handler = std::make_unique<DefaultPullAckHandler>(
      cq, mock, subscription, "test-ack-id", 42,
      std::make_shared<MockPullLeaseManager>());

  EXPECT_EQ(handler->subscription(), subscription);
}

TEST(AckHandlerTest, AckId) {
  auto mock = std::make_shared<MockSubscriberStub>();
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto handler = std::make_unique<DefaultPullAckHandler>(
      cq, mock, pubsub::Subscription("test-project", "test-subscription"),
      "test-ack-id", 42, std::make_shared<MockPullLeaseManager>());

  EXPECT_EQ(handler->ack_id(), "test-ack-id");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
