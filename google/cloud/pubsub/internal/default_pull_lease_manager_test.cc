// Copyright 2023 Google LLC
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

#include "google/cloud/pubsub/internal/default_pull_lease_manager.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/pubsub/retry_policy.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/fake_clock.h"
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
using ::google::cloud::testing_util::FakeSystemClock;
using ::google::cloud::testing_util::StatusIs;
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

Status PermanentError() {
  return Status(StatusCode::kPermissionDenied, "uh-oh");
}

TEST(DefaultPullLeaseManager, SimpleLeaseLoop) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");

  auto const start = std::chrono::system_clock::now();
  auto current_time = start;
  auto clock = std::make_shared<FakeSystemClock>();
  clock->SetTime(current_time);
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
              AsyncModifyAckDeadline(_, context_matcher, _, request_matcher))
      .WillOnce([&] {
        return aseq.PushBack("AsyncModifyAckDeadline").then([](auto f) {
          return f.get() ? Status{} : PermanentError();
        });
      });
  EXPECT_CALL(*mock, AsyncModifyAckDeadline).WillOnce([&] {
    return aseq.PushBack("AsyncModifyAckDeadline").then([](auto f) {
      return f.get() ? Status{} : PermanentError();
    });
  });
  auto last_context_matcher = Pointee(Property(
      &grpc::ClientContext::deadline, Le(current_time + kLeaseDeadline)));
  auto last_request_matcher = AllOf(
      Property(&ModifyAckDeadlineRequest::ack_ids, ElementsAre("test-ack-id")),
      // See MakeTestOptions()
      Property(&ModifyAckDeadlineRequest::ack_deadline_seconds,
               kLastLeaseExtension.count()),
      Property(&ModifyAckDeadlineRequest::subscription,
               subscription.FullName()));
  EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, last_context_matcher, _,
                                            last_request_matcher))
      .WillOnce([&] {
        return aseq.PushBack("AsyncModifyAckDeadline").then([](auto f) {
          return f.get() ? Status{} : PermanentError();
        });
      });
  auto manager = std::make_shared<DefaultPullLeaseManager>(
      cq, mock, options, subscription, "test-ack-id",
      std::make_shared<DefaultPullLeaseManagerImpl>(), clock);
  manager->StartLeaseLoop();
  auto pending = aseq.PopFrontWithName();
  EXPECT_EQ(pending.second, "AsyncModifyAckDeadline");
  pending.first.set_value(true);
  pending = aseq.PopFrontWithName();
  EXPECT_EQ(pending.second, "MakeRelativeTimer");
  EXPECT_THAT(manager->current_lease(), clock->Now() + kLeaseExtension);

  clock->AdvanceTime(kLeaseExtension - kLeaseSlack);
  pending.first.set_value(true);
  pending = aseq.PopFrontWithName();
  EXPECT_EQ(pending.second, "AsyncModifyAckDeadline");
  pending.first.set_value(true);
  pending = aseq.PopFrontWithName();
  EXPECT_EQ(pending.second, "MakeRelativeTimer");
  EXPECT_THAT(manager->current_lease(), clock->Now() + kLeaseExtension);

  // This is close to the end of the lifetime
  clock->AdvanceTime(kLeaseExtension - kLeaseSlack);
  pending.first.set_value(true);
  pending = aseq.PopFrontWithName();
  EXPECT_EQ(pending.second, "AsyncModifyAckDeadline");
  pending.first.set_value(true);
  pending = aseq.PopFrontWithName();
  EXPECT_EQ(pending.second, "MakeRelativeTimer");
  EXPECT_THAT(manager->current_lease(), clock->Now() + kLastLeaseExtension);

  // Terminate the loop. With exceptions disabled abandoning a future with a
  // continuation results in a crash. In non-test programs, the completion queue
  // does this automatically as part of its shutdown.
  pending.first.set_value(false);
}

TEST(DefaultPullLeaseManager, StartLeaseLoopAlreadyReleased) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");

  auto current_time = std::chrono::system_clock::now();
  auto clock = std::make_shared<FakeSystemClock>();
  clock->SetTime(current_time);
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto mock = std::make_shared<MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncModifyAckDeadline).Times(0);
  auto manager = std::make_shared<DefaultPullLeaseManager>(
      cq, mock, MakeTestOptions(), subscription, "test-ack-id",
      std::make_shared<DefaultPullLeaseManagerImpl>(), std::move(clock));
  // This can happen if the subscriber is shutdown, but the application manages
  // to hold to a `AckHandler` reference. In this case, we expect the loop to
  // stop (or have no effect).
  mock.reset();
  manager->StartLeaseLoop();
}

TEST(DefaultPullLeaseManager, StartLeaseLoopAlreadyPastMaxExtension) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");

  auto current_time = std::chrono::system_clock::now();
  auto clock = std::make_shared<FakeSystemClock>();
  clock->SetTime(current_time);
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto mock = std::make_shared<MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncModifyAckDeadline).Times(0);
  auto manager = std::make_shared<DefaultPullLeaseManager>(
      cq, mock, MakeTestOptions(), subscription, "test-ack-id",
      std::make_shared<DefaultPullLeaseManagerImpl>(), clock);
  EXPECT_THAT(manager->lease_deadline(),
              Eq(current_time + std::chrono::seconds(300)));
  // See the MakeTestOptions() for the magic number.
  clock->AdvanceTime(std::chrono::seconds(301));
  manager->StartLeaseLoop();
  // This is a "AsyncModifyAckDeadline() is not called" test.
}

TEST(DefaultPullLeaseManager, StartLeaseLoopTooCloseMaxExtension) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");

  auto current_time = std::chrono::system_clock::now();
  auto clock = std::make_shared<FakeSystemClock>();
  clock->SetTime(current_time);
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto mock = std::make_shared<MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncModifyAckDeadline).Times(0);
  auto manager = std::make_shared<DefaultPullLeaseManager>(
      cq, mock, MakeTestOptions(), subscription, "test-ack-id",
      std::make_shared<DefaultPullLeaseManagerImpl>(), clock);
  EXPECT_THAT(manager->lease_deadline(),
              Eq(current_time + std::chrono::seconds(300)));
  // See the MakeTestOptions() for the magic number.
  clock->AdvanceTime(std::chrono::seconds(299) +
                     std::chrono::milliseconds(500));
  manager->StartLeaseLoop();
  // This is a "AsyncModifyAckDeadline() is not called" test.
}

TEST(DefaultPullLeaseManager, StartLeaseLoopAlreadyPastCurrentExtension) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");

  auto current_time = std::chrono::system_clock::now();
  auto clock = std::make_shared<FakeSystemClock>();
  clock->SetTime(current_time);
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto mock = std::make_shared<MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncModifyAckDeadline).Times(0);
  auto manager = std::make_shared<DefaultPullLeaseManager>(
      cq, mock, MakeTestOptions(), subscription, "test-ack-id",
      std::make_shared<DefaultPullLeaseManagerImpl>(), clock);
  EXPECT_GT(manager->current_lease(), current_time);
  clock->SetTime(manager->current_lease());
  manager->StartLeaseLoop();
  // This is a "AsyncModifyAckDeadline() is not called" test.
}

TEST(DefaultPullLeaseManager, InitializeDeadlines) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");

  auto current_time = std::chrono::system_clock::now();
  auto clock = std::make_shared<FakeSystemClock>();
  clock->SetTime(current_time);
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto mock = std::make_shared<MockSubscriberStub>();

  auto manager = std::make_shared<DefaultPullLeaseManager>(
      cq, mock,
      google::cloud::pubsub_testing::MakeTestOptions(
          Options{}
              .set<pubsub::MaxDeadlineTimeOption>(std::chrono::seconds(300))
              .set<pubsub::MinDeadlineExtensionOption>(
                  std::chrono::seconds(10))),
      subscription, "test-ack-id",
      std::make_shared<DefaultPullLeaseManagerImpl>(), clock);
  EXPECT_EQ(manager->lease_deadline(),
            current_time + std::chrono::seconds(300));
  EXPECT_EQ(manager->LeaseRefreshPeriod(), std::chrono::seconds(9));

  manager = std::make_shared<DefaultPullLeaseManager>(
      cq, mock,
      google::cloud::pubsub_testing::MakeTestOptions(
          Options{}
              .set<pubsub::MaxDeadlineTimeOption>(std::chrono::seconds(300))
              .set<pubsub::MaxDeadlineExtensionOption>(
                  std::chrono::seconds(30))),
      subscription, "test-ack-id",
      std::make_shared<DefaultPullLeaseManagerImpl>(), clock);
  EXPECT_EQ(manager->lease_deadline(),
            current_time + std::chrono::seconds(300));
  EXPECT_EQ(manager->LeaseRefreshPeriod(), std::chrono::seconds(29));

  manager = std::make_shared<DefaultPullLeaseManager>(
      cq, mock,
      google::cloud::pubsub_testing::MakeTestOptions(
          Options{}
              .set<pubsub::MaxDeadlineTimeOption>(std::chrono::seconds(300))
              .set<pubsub::MinDeadlineExtensionOption>(std::chrono::seconds(10))
              .set<pubsub::MaxDeadlineExtensionOption>(
                  std::chrono::seconds(30))),
      subscription, "test-ack-id",
      std::make_shared<DefaultPullLeaseManagerImpl>(), clock);
  EXPECT_EQ(manager->lease_deadline(),
            current_time + std::chrono::seconds(300));
  EXPECT_EQ(manager->LeaseRefreshPeriod(), std::chrono::seconds(9));
}

TEST(DefaultPullLeaseManager, ExtendLeaseDeadlineSimple) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  auto constexpr kLeaseExtension = std::chrono::seconds(10);
  auto options = google::cloud::pubsub_testing::MakeTestOptions(
      Options{}.set<pubsub::MaxDeadlineExtensionOption>(kLeaseExtension));

  auto mock = std::make_shared<MockSubscriberStub>();
  auto request_matcher = AllOf(
      Property(&ModifyAckDeadlineRequest::ack_ids, ElementsAre("test-ack-id")),
      Property(&ModifyAckDeadlineRequest::ack_deadline_seconds,
               kLeaseExtension.count()),
      Property(&ModifyAckDeadlineRequest::subscription,
               subscription.FullName()));
  EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _, _, request_matcher))
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto current_time = std::chrono::system_clock::now();
  auto clock = std::make_shared<FakeSystemClock>();
  clock->SetTime(current_time);
  auto manager = std::make_shared<DefaultPullLeaseManager>(
      cq, mock, options, subscription, "test-ack-id",
      std::make_shared<DefaultPullLeaseManagerImpl>(), std::move(clock));

  auto status = manager->ExtendLease(mock, current_time, kLeaseExtension);
  EXPECT_STATUS_OK(status.get());
}

TEST(DefaultPullLeaseManager, ExtendLeaseDeadlineExceeded) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  auto constexpr kLeaseExtension = std::chrono::seconds(10);
  auto options = google::cloud::pubsub_testing::MakeTestOptions(
      Options{}.set<pubsub::MaxDeadlineExtensionOption>(kLeaseExtension));

  auto mock = std::make_shared<MockSubscriberStub>();
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto current_time = std::chrono::system_clock::now();
  auto clock = std::make_shared<FakeSystemClock>();
  // Set the time for the clock call to after the current time +
  // extension.
  clock->SetTime(current_time + std::chrono::seconds(11));
  auto manager = std::make_shared<DefaultPullLeaseManager>(
      cq, mock, options, subscription, "test-ack-id",
      std::make_shared<DefaultPullLeaseManagerImpl>(), std::move(clock));

  auto status = manager->ExtendLease(mock, current_time, kLeaseExtension);
  EXPECT_THAT(status.get(),
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("expired")));
}

TEST(DefaultPullLeaseManager, ExtendLeasePermanentError) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  auto constexpr kLeaseExtension = std::chrono::seconds(10);
  auto options = google::cloud::pubsub_testing::MakeTestOptions(
      Options{}.set<pubsub::MaxDeadlineExtensionOption>(kLeaseExtension));

  auto mock = std::make_shared<MockSubscriberStub>();
  auto request_matcher = AllOf(
      Property(&ModifyAckDeadlineRequest::ack_ids, ElementsAre("test-ack-id")),
      Property(&ModifyAckDeadlineRequest::ack_deadline_seconds,
               kLeaseExtension.count()),
      Property(&ModifyAckDeadlineRequest::subscription,
               subscription.FullName()));
  EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _, _, request_matcher))
      .WillOnce(Return(ByMove(make_ready_future(PermanentError()))));
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto current_time = std::chrono::system_clock::now();
  auto clock = std::make_shared<FakeSystemClock>();
  clock->SetTime(current_time);
  auto manager = std::make_shared<DefaultPullLeaseManager>(
      cq, mock, options, subscription, "test-ack-id",
      std::make_shared<DefaultPullLeaseManagerImpl>(), std::move(clock));

  auto status = manager->ExtendLease(mock, current_time, kLeaseExtension);
  EXPECT_THAT(status.get(),
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

TEST(DefaultPullLeaseManager, Subscription) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  auto mock = std::make_shared<MockSubscriberStub>();
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto clock = std::make_shared<FakeSystemClock>();
  clock->SetTime(std::chrono::system_clock::now());
  auto manager = std::make_shared<DefaultPullLeaseManager>(
      cq, mock, Options{}, subscription, "test-ack-id",
      std::make_shared<DefaultPullLeaseManagerImpl>(), std::move(clock));

  EXPECT_EQ(manager->subscription(), subscription);
}

TEST(DefaultPullLeaseManager, AckId) {
  auto mock = std::make_shared<MockSubscriberStub>();
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto clock = std::make_shared<FakeSystemClock>();
  clock->SetTime(std::chrono::system_clock::now());
  auto manager = std::make_shared<DefaultPullLeaseManager>(
      cq, mock, Options{},
      pubsub::Subscription("test-project", "test-subscription"), "test-ack-id",
      std::make_shared<DefaultPullLeaseManagerImpl>(), std::move(clock));

  EXPECT_EQ(manager->ack_id(), "test-ack-id");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
