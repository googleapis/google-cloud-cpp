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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/pubsub/internal/tracing_pull_ack_handler.h"
#include "google/cloud/pubsub/mocks/mock_pull_ack_handler.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <opentelemetry/trace/semantic_conventions.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::pubsub_mocks::MockPullAckHandler;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Return;

pubsub::Subscription TestSubscription() {
  return pubsub::Subscription("test-project", "test-subscription");
}

std::string TestAckId() { return "test-ack-id"; }

std::unique_ptr<pubsub::PullAckHandler::Impl> MakeTestPullAckHandler(
    std::unique_ptr<pubsub::PullAckHandler::Impl> mock) {
  return MakeTracingPullAckHandler(std::move(mock), TestSubscription(),
                                   TestAckId());
}

TEST(TracingAckHandlerTest, AckSuccess) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullAckHandler>();
  EXPECT_CALL(*mock, ack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  EXPECT_CALL(*mock, delivery_attempt()).WillOnce(Return(42));
  auto handler = MakeTestPullAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler)->ack().get(), StatusIs(StatusCode::kOk));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanHasInstrumentationScope(), SpanKindIsClient(),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                         SpanNamed("test-subscription settle"))));
}

TEST(TracingAckHandlerTest, AckError) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullAckHandler>();
  EXPECT_CALL(*mock, ack())
      .WillOnce(Return(ByMove(
          make_ready_future(Status{StatusCode::kPermissionDenied, "uh-oh"}))));
  EXPECT_CALL(*mock, delivery_attempt()).WillOnce(Return(42));
  auto handler = MakeTestPullAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler)->ack().get(),
              StatusIs(StatusCode::kPermissionDenied, "uh-oh"));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(SpanWithStatus(opentelemetry::trace::StatusCode::kError),
                     SpanNamed("test-subscription settle"))));
}

TEST(TracingAckHandlerTest, AckAttributes) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullAckHandler>();
  EXPECT_CALL(*mock, ack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  EXPECT_CALL(*mock, delivery_attempt()).WillOnce(Return(42));
  auto handler = MakeTestPullAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler->ack()).get(), StatusIs(StatusCode::kOk));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription settle"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kMessagingSystem, "gcp_pubsub")))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription settle"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kMessagingOperation, "settle")))));
  EXPECT_THAT(
      spans,
      Contains(AllOf(SpanNamed("test-subscription settle"),
                     SpanHasAttributes(OTelAttribute<std::string>(
                         sc::kCodeFunction, "pubsub::PullAckHandler::ack")))));
  EXPECT_THAT(spans,
              Contains(AllOf(
                  SpanNamed("test-subscription settle"),
                  SpanHasAttributes(OTelAttribute<std::int32_t>(
                      "messaging.gcp_pubsub.message.delivery_attempt", 42)))));
}

TEST(TracingAckHandlerTest, NackSuccess) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullAckHandler>();
  EXPECT_CALL(*mock, nack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  EXPECT_CALL(*mock, delivery_attempt()).WillOnce(Return(42));
  auto handler = MakeTestPullAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler)->nack().get(), StatusIs(StatusCode::kOk));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanHasInstrumentationScope(), SpanKindIsClient(),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                         SpanNamed("test-subscription settle"))));
}

TEST(TracingAckHandlerTest, NackError) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullAckHandler>();
  EXPECT_CALL(*mock, nack())
      .WillOnce(Return(ByMove(
          make_ready_future(Status{StatusCode::kPermissionDenied, "uh-oh"}))));
  EXPECT_CALL(*mock, delivery_attempt()).WillOnce(Return(42));
  auto handler = MakeTestPullAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler)->nack().get(),
              StatusIs(StatusCode::kPermissionDenied, "uh-oh"));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(SpanWithStatus(opentelemetry::trace::StatusCode::kError),
                     SpanNamed("test-subscription settle"))));
}

TEST(TracingAckHandlerTest, NackAttributes) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullAckHandler>();
  EXPECT_CALL(*mock, nack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  EXPECT_CALL(*mock, delivery_attempt()).WillOnce(Return(42));
  auto handler = MakeTestPullAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler->nack()).get(), StatusIs(StatusCode::kOk));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription settle"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kMessagingSystem, "gcp_pubsub")))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription settle"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kMessagingOperation, "settle")))));
  EXPECT_THAT(
      spans,
      Contains(AllOf(SpanNamed("test-subscription settle"),
                     SpanHasAttributes(OTelAttribute<std::string>(
                         sc::kCodeFunction, "pubsub::PullAckHandler::nack")))));
  EXPECT_THAT(spans,
              Contains(AllOf(
                  SpanNamed("test-subscription settle"),
                  SpanHasAttributes(OTelAttribute<std::int32_t>(
                      "messaging.gcp_pubsub.message.delivery_attempt", 42)))));
}

TEST(TracingAckHandlerTest, DeliveryAttemptNoSpans) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullAckHandler>();
  EXPECT_CALL(*mock, delivery_attempt()).WillOnce(Return(42));
  auto handler = MakeTestPullAckHandler(std::move(mock));

  EXPECT_EQ(42, handler->delivery_attempt());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, IsEmpty());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
