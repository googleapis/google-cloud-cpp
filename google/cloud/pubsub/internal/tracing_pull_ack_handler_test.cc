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
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Return;

pubsub::Subscription const kTestSubscription =
    pubsub::Subscription("test-project", "test-subscription");
auto constexpr kTestAckId = "test-ack-id";

std::unique_ptr<pubsub::PullAckHandler::Impl> MakeTestPullAckHandler(
    std::unique_ptr<MockPullAckHandler> mock) {
  EXPECT_CALL(*mock, delivery_attempt()).WillRepeatedly(Return(42));
  EXPECT_CALL(*mock, ack_id()).WillRepeatedly(Return(kTestAckId));
  EXPECT_CALL(*mock, subscription()).WillRepeatedly(Return(kTestSubscription));

  return MakeTracingPullAckHandler(std::move(mock));
}

TEST(TracingAckHandlerTest, AckSuccess) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullAckHandler>();
  EXPECT_CALL(*mock, ack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  auto handler = MakeTestPullAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler)->ack().get(), StatusIs(StatusCode::kOk));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanHasInstrumentationScope(), SpanKindIsClient(),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                         SpanNamed("test-subscription ack"))));
}

TEST(TracingAckHandlerTest, AckError) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullAckHandler>();
  EXPECT_CALL(*mock, ack())
      .WillOnce(Return(ByMove(
          make_ready_future(Status{StatusCode::kPermissionDenied, "uh-oh"}))));
  auto handler = MakeTestPullAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler)->ack().get(),
              StatusIs(StatusCode::kPermissionDenied, "uh-oh"));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(SpanWithStatus(opentelemetry::trace::StatusCode::kError),
                     SpanNamed("test-subscription ack"))));
}

TEST(TracingAckHandlerTest, AckAttributes) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullAckHandler>();
  EXPECT_CALL(*mock, ack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  auto handler = MakeTestPullAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler->ack()).get(), StatusIs(StatusCode::kOk));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription ack"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kMessagingSystem, "gcp_pubsub")))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription ack"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 "gcp.project_id", "test-project")))));
  EXPECT_THAT(
      spans, Contains(AllOf(
                 SpanNamed("test-subscription ack"),
                 SpanHasAttributes(OTelAttribute<std::string>(
                     /*sc::kMessagingOperationType=*/"messaging.operation.type",
                     "ack")))));
  EXPECT_THAT(
      spans,
      Contains(AllOf(SpanNamed("test-subscription ack"),
                     SpanHasAttributes(OTelAttribute<std::string>(
                         sc::kCodeFunction, "pubsub::PullAckHandler::ack")))));
  EXPECT_THAT(spans,
              Contains(AllOf(
                  SpanNamed("test-subscription ack"),
                  SpanHasAttributes(OTelAttribute<std::int32_t>(
                      "messaging.gcp_pubsub.message.delivery_attempt", 42)))));
  EXPECT_THAT(spans,
              Contains(AllOf(
                  SpanNamed("test-subscription ack"),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      sc::kMessagingDestinationName, "test-subscription")))));
}

TEST(TracingAckHandlerTest, NackSuccess) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullAckHandler>();
  EXPECT_CALL(*mock, nack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  auto handler = MakeTestPullAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler)->nack().get(), StatusIs(StatusCode::kOk));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanHasInstrumentationScope(), SpanKindIsClient(),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                         SpanNamed("test-subscription nack"))));
}

TEST(TracingAckHandlerTest, NackError) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullAckHandler>();
  EXPECT_CALL(*mock, nack())
      .WillOnce(Return(ByMove(
          make_ready_future(Status{StatusCode::kPermissionDenied, "uh-oh"}))));
  auto handler = MakeTestPullAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler)->nack().get(),
              StatusIs(StatusCode::kPermissionDenied, "uh-oh"));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(SpanWithStatus(opentelemetry::trace::StatusCode::kError),
                     SpanNamed("test-subscription nack"))));
}

TEST(TracingAckHandlerTest, NackAttributes) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullAckHandler>();
  EXPECT_CALL(*mock, nack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  auto handler = MakeTestPullAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler->nack()).get(), StatusIs(StatusCode::kOk));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription nack"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kMessagingSystem, "gcp_pubsub")))));
  EXPECT_THAT(
      spans, Contains(AllOf(
                 SpanNamed("test-subscription nack"),
                 SpanHasAttributes(OTelAttribute<std::string>(
                     /*sc::kMessagingOperationType=*/"messaging.operation.type",
                     "nack")))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription nack"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 "gcp.project_id", "test-project")))));
  EXPECT_THAT(
      spans,
      Contains(AllOf(SpanNamed("test-subscription nack"),
                     SpanHasAttributes(OTelAttribute<std::string>(
                         sc::kCodeFunction, "pubsub::PullAckHandler::nack")))));
  EXPECT_THAT(spans,
              Contains(AllOf(
                  SpanNamed("test-subscription nack"),
                  SpanHasAttributes(OTelAttribute<std::int32_t>(
                      "messaging.gcp_pubsub.message.delivery_attempt", 42)))));
  EXPECT_THAT(spans,
              Contains(AllOf(
                  SpanNamed("test-subscription nack"),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      sc::kMessagingDestinationName, "test-subscription")))));
}

TEST(TracingAckHandlerTest, DeliveryAttemptNoSpans) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullAckHandler>();
  auto handler = MakeTestPullAckHandler(std::move(mock));

  EXPECT_EQ(42, handler->delivery_attempt());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, IsEmpty());
}

TEST(TracingAckHandlerTest, AckIdNoSpans) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullAckHandler>();
  auto handler = MakeTestPullAckHandler(std::move(mock));

  EXPECT_EQ(kTestAckId, handler->ack_id());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, IsEmpty());
}

TEST(TracingAckHandlerTest, SubscriptionNoSpans) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullAckHandler>();
  auto handler = MakeTestPullAckHandler(std::move(mock));

  EXPECT_EQ(kTestSubscription, handler->subscription());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, IsEmpty());
}

#if OPENTELEMETRY_ABI_VERSION_NO >= 2

using ::google::cloud::testing_util::SpanLinksSizeIs;

TEST(TracingAckHandlerTest, AckAddsLink) {
  auto span_catcher = InstallSpanCatcher();
  auto consumer_span = internal::MakeSpan("receive");
  auto scope = internal::OTelScope(consumer_span);
  auto mock = std::make_unique<MockPullAckHandler>();
  EXPECT_CALL(*mock, ack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  auto handler = MakeTestPullAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler->ack()).get(), StatusIs(StatusCode::kOk));

  consumer_span->End();
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(SpanNamed("test-subscription ack"),
                                    SpanLinksSizeIs(1))));
}

TEST(TracingAckHandlerTest, AckSkipsLinkForNotSampledSpan) {
  // Create the span before it is sampled so no link is added.
  auto consumer_span = internal::MakeSpan("receive");
  auto span_catcher = InstallSpanCatcher();
  auto scope = internal::OTelScope(consumer_span);
  auto mock = std::make_unique<MockPullAckHandler>();
  EXPECT_CALL(*mock, ack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  auto handler = MakeTestPullAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler->ack()).get(), StatusIs(StatusCode::kOk));

  consumer_span->End();
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(SpanNamed("test-subscription ack"),
                                    SpanLinksSizeIs(0))));
}

#else

TEST(TracingAckHandlerTest, AckAddsSpanIdAndTraceIdAttribute) {
  auto span_catcher = InstallSpanCatcher();
  auto consumer_span = internal::MakeSpan("receive");
  auto scope = internal::OTelScope(consumer_span);
  auto mock = std::make_unique<MockPullAckHandler>();
  EXPECT_CALL(*mock, ack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  auto handler = MakeTestPullAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler->ack()).get(), StatusIs(StatusCode::kOk));

  consumer_span->End();
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(
          SpanNamed("test-subscription ack"),
          SpanHasAttributes(
              OTelAttribute<std::string>("gcp_pubsub.receive.trace_id", _),
              OTelAttribute<std::string>("gcp_pubsub.receive.span_id", _)))));
}

#endif

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
