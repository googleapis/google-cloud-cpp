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

#include "google/cloud/pubsub/internal/pull_ack_handler_factory.h"
#include "google/cloud/pubsub/mocks/mock_pull_ack_handler.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/future_generic.h"
#include "google/cloud/testing_util/async_sequencer.h"
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
using ::google::cloud::testing_util::StatusIs;
using ::google::pubsub::v1::AcknowledgeRequest;
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

TEST(PullAckHandlerTest, AckSimple) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");

  auto mock = std::make_shared<MockSubscriberStub>();
  auto request_matcher = AllOf(
      Property(&AcknowledgeRequest::ack_ids, ElementsAre("test-ack-id")),
      Property(&AcknowledgeRequest::subscription, subscription.FullName()));
  EXPECT_CALL(*mock, AsyncAcknowledge).WillRepeatedly([] {
    return make_ready_future(Status{});
  });
  EXPECT_CALL(*mock, AsyncModifyAckDeadline).WillRepeatedly([] {
    return make_ready_future(Status{});
  });
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto handler =
      MakePullAckHandler(std::move(cq), std::move(mock), subscription,
                         "test-ack-id", 42, MakeTestOptions());
  auto pending = aseq.PopFrontWithName();
  EXPECT_EQ(pending.second, "MakeRelativeTimer");
  pending.first.set_value(true);
  pending = aseq.PopFrontWithName();
  EXPECT_EQ(pending.second, "MakeRelativeTimer");
  EXPECT_THAT(std::move(handler).ack().get(), StatusIs(StatusCode::kOk));

  // Terminate the loop. With exceptions disabled abandoning a future with a
  // continuation results in a crash. In non-test programs, the completion queue
  // does this automatically as part of its shutdown.
  pending.first.set_value(false);
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

TEST(PullAckHandlerTest, TracingEnabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  auto mock = std::make_shared<MockSubscriberStub>();
  auto request_matcher = AllOf(
      Property(&AcknowledgeRequest::ack_ids, ElementsAre("test-ack-id")),
      Property(&AcknowledgeRequest::subscription, subscription.FullName()));
  EXPECT_CALL(*mock, AsyncAcknowledge(_, _, _, request_matcher))
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  // Since the lease manager is started in the constructor of the ack handler,
  // we need to match the lease manager calls.
  EXPECT_CALL(*mock, AsyncModifyAckDeadline).WillRepeatedly([] {
    return make_ready_future(Status{});
  });
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto handler =
      MakePullAckHandler(std::move(cq), std::move(mock), subscription,
                         "test-ack-id", 42, EnableTracing(MakeTestOptions()));
  auto pending = aseq.PopFrontWithName();
  EXPECT_EQ(pending.second, "MakeRelativeTimer");
  pending.first.set_value(true);
  pending = aseq.PopFrontWithName();
  EXPECT_EQ(pending.second, "MakeRelativeTimer");

  EXPECT_THAT(std::move(handler).ack().get(), StatusIs(StatusCode::kOk));

  // Terminate the loop. With exceptions disabled abandoning a future with a
  // continuation results in a crash. In non-test programs, the completion queue
  // does this automatically as part of its shutdown.
  pending.first.set_value(false);
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanHasInstrumentationScope(), SpanKindIsClient(),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                         SpanNamed("test-subscription ack"))));
}

TEST(PullAckHandlerTest, TracingDisabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  auto mock = std::make_shared<MockSubscriberStub>();
  auto request_matcher = AllOf(
      Property(&AcknowledgeRequest::ack_ids, ElementsAre("test-ack-id")),
      Property(&AcknowledgeRequest::subscription, subscription.FullName()));
  EXPECT_CALL(*mock, AsyncAcknowledge(_, _, _, request_matcher))
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  // Since the lease manager is started in the constructor of the ack handler,
  // we need to match the lease manager calls.
  EXPECT_CALL(*mock, AsyncModifyAckDeadline).WillRepeatedly([]() {
    return make_ready_future(Status{});
  });
  AsyncSequencer<bool> aseq;
  auto cq = MakeMockCompletionQueue(aseq);
  auto handler =
      MakePullAckHandler(std::move(cq), std::move(mock), subscription,
                         "test-ack-id", 42, DisableTracing(MakeTestOptions()));
  auto pending = aseq.PopFrontWithName();
  EXPECT_EQ(pending.second, "MakeRelativeTimer");
  pending.first.set_value(true);
  pending = aseq.PopFrontWithName();
  EXPECT_EQ(pending.second, "MakeRelativeTimer");

  EXPECT_THAT(std::move(handler).ack().get(), StatusIs(StatusCode::kOk));
  // Terminate the loop. With exceptions disabled abandoning a future with a
  // continuation results in a crash. In non-test programs, the completion queue
  // does this automatically as part of its shutdown.
  pending.first.set_value(false);

  EXPECT_THAT(span_catcher->GetSpans(), IsEmpty());
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
