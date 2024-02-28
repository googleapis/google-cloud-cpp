// Copyright 2023 Google LLC
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

#include "google/cloud/pubsub/internal/pull_lease_manager_factory.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/fake_clock.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
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
using ::google::pubsub::v1::ModifyAckDeadlineRequest;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::Return;

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

Options MakeTestOptions() {
  return google::cloud::pubsub_testing::MakeTestOptions(
      Options{}
          .set<pubsub::MaxDeadlineTimeOption>(std::chrono::seconds(300))
          .set<pubsub::MaxDeadlineExtensionOption>(std::chrono::seconds(10)));
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
  auto manager =
      MakePullLeaseManager(std::move(cq), std::move(mock), subscription,
                           "test-ack-id", MakeTestOptions(), std::move(clock));

  auto status = manager->ExtendLease(mock, current_time, kLeaseExtension);
  EXPECT_STATUS_OK(status.get());
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(DefaultPullLeaseManager, TracingEnabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();
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
  auto manager = MakePullLeaseManager(
      std::move(cq), std::move(mock), subscription, "test-ack-id",
      EnableTracing(MakeTestOptions()), std::move(clock));

  auto status = manager->ExtendLease(mock, current_time, kLeaseExtension);
  EXPECT_STATUS_OK(status.get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanHasInstrumentationScope(), SpanKindIsClient(),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                         SpanNamed("test-subscription modack"))));
}

TEST(DefaultPullLeaseManager, TracingDisabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();
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
  auto manager = MakePullLeaseManager(
      std::move(cq), std::move(mock), subscription, "test-ack-id",
      DisableTracing(MakeTestOptions()), std::move(clock));

  auto status = manager->ExtendLease(mock, current_time, kLeaseExtension);
  EXPECT_STATUS_OK(status.get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(span_catcher->GetSpans(), IsEmpty());
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
