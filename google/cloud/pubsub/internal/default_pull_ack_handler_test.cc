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
#include "google/cloud/pubsub/options.h"
#include "google/cloud/pubsub/retry_policy.h"
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

using ::google::cloud::pubsub_testing::MockSubscriberStub;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::StatusIs;
using ::google::pubsub::v1::AcknowledgeRequest;
using ::google::pubsub::v1::ModifyAckDeadlineRequest;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Le;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

using MockClock =
    ::testing::MockFunction<std::chrono::system_clock::time_point()>;

Options MakeTestOptions() {
  return google::cloud::pubsub_testing::MakeTestOptions(
      Options{}
          .set<pubsub::MaxDeadlineTimeOption>(std::chrono::seconds(300))
          .set<pubsub::MaxDeadlineExtensionOption>(std::chrono::seconds(10)));
}

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
  // Start lease loop past max extension so that we don't get any modify ack
  // deadline calls.
  auto const current_time = std::chrono::system_clock::now();
  MockClock clock;
  // On the first call, when the class is created return the current time. Any
  // calls after will show the current time is past the max deadline. See
  // MakeTestOptions for the magic numbers.
  EXPECT_CALL(clock, Call)
      .WillOnce([&] { return current_time; })
      .WillRepeatedly([&] { return current_time + std::chrono::seconds(301); });

  auto mock = std::make_shared<MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _, _)).Times(0);
  // Expect the ack call.
  auto request_matcher = AllOf(
      Property(&AcknowledgeRequest::ack_ids, ElementsAre("test-ack-id")),
      Property(&AcknowledgeRequest::subscription, subscription.FullName()));
  EXPECT_CALL(*mock, AsyncAcknowledge(_, _, request_matcher))
      .WillOnce(Return(ByMove(make_ready_future(TransientError()))))
      .WillOnce(
          Return(ByMove(make_ready_future(TransientError("test-ack-id")))))
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto handler = std::make_unique<DefaultPullAckHandler>(
      cq, mock, MakeTestOptions(), subscription, "test-ack-id", 42,
      clock.AsStdFunction());
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

TEST(PullAckHandlerTest, AckWithLeaseManagement) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");

  // Use a mock clock where the time is controlled by a test variable. We do not
  // really care how many times or exactly when is the clock called, we just
  // want to control the passage of time.
  auto const start = std::chrono::system_clock::now();
  auto current_time = start;
  MockClock clock;
  EXPECT_CALL(clock, Call).WillRepeatedly([&] { return current_time; });
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto mock = std::make_shared<MockSubscriberStub>();

  // these values explain the magic numbers in the expectations
  auto constexpr kLeaseExtension = std::chrono::seconds(10);
  auto constexpr kLeaseOdd = std::chrono::seconds(3);
  auto constexpr kLeaseDeadline = 2 * kLeaseExtension + kLeaseOdd;
  auto constexpr kLeaseSlack = std::chrono::seconds(1);
  auto constexpr kLastLeaseExtension = 2 * kLeaseSlack + kLeaseOdd;
  auto options = google::cloud::pubsub_testing::MakeTestOptions(
      Options{}
          .set<pubsub::MaxDeadlineTimeOption>(kLeaseDeadline)
          .set<pubsub::MaxDeadlineExtensionOption>(kLeaseExtension));

  // Set the expectation for the lease management calls.
  auto context_matcher = Pointee(Property(&grpc::ClientContext::deadline,
                                          Le(current_time + kLeaseExtension)));
  auto request_matcher = AllOf(
      Property(&ModifyAckDeadlineRequest::ack_ids, ElementsAre("test-ack-id")),
      Property(&ModifyAckDeadlineRequest::ack_deadline_seconds,
               kLeaseExtension.count()),
      Property(&ModifyAckDeadlineRequest::subscription,
               subscription.FullName()));
  ::testing::InSequence sequence;
  EXPECT_CALL(*mock,
              AsyncModifyAckDeadline(_, context_matcher, request_matcher))
      .WillOnce([&](auto, auto, auto) {
        return aseq.PushBack("AsyncModifyAckDeadline").then([](auto f) {
          return f.get() ? Status{} : PermanentError();
        });
      });
  EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _, _))
      .WillOnce([&](auto, auto, auto) {
        return aseq.PushBack("AsyncModifyAckDeadline").then([](auto f) {
          return f.get() ? Status{} : PermanentError();
        });
      });

  // Set the expectation for the ack call.
  auto ack_request_matcher = AllOf(
      Property(&AcknowledgeRequest::ack_ids, ElementsAre("test-ack-id")),
      Property(&AcknowledgeRequest::subscription, subscription.FullName()));
  EXPECT_CALL(*mock, AsyncAcknowledge(_, _, ack_request_matcher))
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));

  // Create the ack handler.
  auto handler = std::make_unique<DefaultPullAckHandler>(
      cq, mock, MakeTestOptions(), subscription, "test-ack-id", 42,
      clock.AsStdFunction());
  // Extend the lease.
  auto pending = aseq.PopFrontWithName();
  EXPECT_EQ(pending.second, "AsyncModifyAckDeadline");
  pending.first.set_value(true);
  pending = aseq.PopFrontWithName();
  EXPECT_EQ(pending.second, "MakeRelativeTimer");
  pending.first.set_value(true);
  pending = aseq.PopFrontWithName();
  EXPECT_EQ(pending.second, "AsyncModifyAckDeadline");
  pending.first.set_value(true);
  pending = aseq.PopFrontWithName();
  EXPECT_EQ(pending.second, "MakeRelativeTimer");
  // Ack the message.
  auto status = handler->ack();
  EXPECT_STATUS_OK(status.get());

  // Terminate the loop. With exceptions disabled abandoning a future with a
  // continuation results in a crash. In non-test programs, the completion queue
  // does this automatically as part of its shutdown.
  pending.first.set_value(false);
}

TEST(PullAckHandlerTest, AckPermanentError) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");

  // Start lease loop past max extension so that we don't get any modify ack
  // deadline calls.
  auto const current_time = std::chrono::system_clock::now();
  MockClock clock;
  // On the first call, when the class is created return the current time. Any
  // calls after will show the current time is past the max deadline. See
  // MakeTestOptions for the magic numbers.
  EXPECT_CALL(clock, Call)
      .WillOnce([&] { return current_time; })
      .WillRepeatedly([&] { return current_time + std::chrono::seconds(301); });

  auto mock = std::make_shared<MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _, _)).Times(0);
  // Expect the ack call.
  auto request_matcher = AllOf(
      Property(&AcknowledgeRequest::ack_ids, ElementsAre("test-ack-id")),
      Property(&AcknowledgeRequest::subscription, subscription.FullName()));
  EXPECT_CALL(*mock, AsyncAcknowledge(_, _, request_matcher))
      .WillOnce(Return(ByMove(make_ready_future(PermanentError()))));
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto handler = std::make_unique<DefaultPullAckHandler>(
      cq, mock, MakeTestOptions(), subscription, "test-ack-id", 42,
      clock.AsStdFunction());
  EXPECT_EQ(handler->delivery_attempt(), 42);
  auto status = handler->ack();
  EXPECT_THAT(status.get(),
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

TEST(PullAckHandlerTest, NackSimple) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  // Start lease loop past max extension so that we don't get any modify ack
  // deadline calls.
  auto const current_time = std::chrono::system_clock::now();
  MockClock clock;
  // On the first call, when the class is created return the current time. Any
  // calls after will show the current time is past the max deadline. See
  // MakeTestOptions for the magic numbers.
  EXPECT_CALL(clock, Call)
      .WillOnce([&] { return current_time; })
      .WillRepeatedly([&] { return current_time + std::chrono::seconds(301); });

  auto mock = std::make_shared<MockSubscriberStub>();
  auto request_matcher = AllOf(
      Property(&ModifyAckDeadlineRequest::ack_ids, ElementsAre("test-ack-id")),
      Property(&ModifyAckDeadlineRequest::ack_deadline_seconds, 0),
      Property(&ModifyAckDeadlineRequest::subscription,
               subscription.FullName()));
  EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _, request_matcher))
      .WillOnce(Return(ByMove(make_ready_future(TransientError()))))
      .WillOnce(
          Return(ByMove(make_ready_future(TransientError("test-ack-id")))))
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto handler = std::make_unique<DefaultPullAckHandler>(
      cq, mock, MakeTestOptions(), subscription, "test-ack-id", 42,
      clock.AsStdFunction());
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
  // Start lease loop past max extension so that we don't get any modify ack
  // deadline calls.
  auto const current_time = std::chrono::system_clock::now();
  MockClock clock;
  // On the first call, when the class is created return the current time. Any
  // calls after will show the current time is past the max deadline. See
  // MakeTestOptions for the magic numbers.
  EXPECT_CALL(clock, Call)
      .WillOnce([&] { return current_time; })
      .WillRepeatedly([&] { return current_time + std::chrono::seconds(301); });

  auto mock = std::make_shared<MockSubscriberStub>();
  auto request_matcher = AllOf(
      Property(&ModifyAckDeadlineRequest::ack_ids, ElementsAre("test-ack-id")),
      Property(&ModifyAckDeadlineRequest::ack_deadline_seconds, 0),
      Property(&ModifyAckDeadlineRequest::subscription,
               subscription.FullName()));
  EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _, request_matcher))
      .WillOnce(Return(ByMove(make_ready_future(PermanentError()))));
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto handler = std::make_unique<DefaultPullAckHandler>(
      cq, mock, MakeTestOptions(), subscription, "test-ack-id", 42,
      clock.AsStdFunction());
  EXPECT_EQ(handler->delivery_attempt(), 42);
  auto status = handler->nack();
  EXPECT_THAT(status.get(),
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

TEST(AckHandlerTest, Subscription) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  // Start lease loop past max extension so that we don't get any modify ack
  // deadline calls.
  auto const current_time = std::chrono::system_clock::now();
  MockClock clock;
  // On the first call, when the class is created return the current time. Any
  // calls after will show the current time is past the max deadline. See
  // MakeTestOptions for the magic numbers.
  EXPECT_CALL(clock, Call)
      .WillOnce([&] { return current_time; })
      .WillRepeatedly([&] { return current_time + std::chrono::seconds(301); });
  auto mock = std::make_shared<MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _, _)).Times(0);
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto handler = std::make_unique<DefaultPullAckHandler>(
      cq, mock, MakeTestOptions(), subscription, "test-ack-id", 42,
      clock.AsStdFunction());

  EXPECT_EQ(handler->subscription(), subscription);
}

TEST(AckHandlerTest, AckId) {
  // Start lease loop past max extension so that we don't get any modify ack
  // deadline calls.
  auto const current_time = std::chrono::system_clock::now();
  MockClock clock;
  // On the first call, when the class is created return the current time. Any
  // calls after will show the current time is past the max deadline. See
  // MakeTestOptions for the magic numbers.
  EXPECT_CALL(clock, Call)
      .WillOnce([&] { return current_time; })
      .WillRepeatedly([&] { return current_time + std::chrono::seconds(301); });
  auto mock = std::make_shared<MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _, _)).Times(0);
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto handler = std::make_unique<DefaultPullAckHandler>(
      cq, mock, MakeTestOptions(),
      pubsub::Subscription("test-project", "test-subscription"), "test-ack-id",
      42, clock.AsStdFunction());

  EXPECT_EQ(handler->ack_id(), "test-ack-id");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
