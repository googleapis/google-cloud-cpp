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

#include "google/cloud/pubsub/internal/tracing_exactly_once_ack_handler.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/testing/mock_exactly_once_ack_handler_impl.h"
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

using ::google::cloud::pubsub_testing::MockExactlyOnceAckHandlerImpl;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsInternal;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::SizeIs;

pubsub::Subscription const kTestSubscription =
    pubsub::Subscription("test-project", "test-subscription");
auto constexpr kTestAckId = "test-ack-id";

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> MakeTestSpan() {
  return internal::GetTracer(internal::CurrentOptions())
      ->StartSpan("test-subscription subscribe");
}

std::unique_ptr<pubsub::ExactlyOnceAckHandler::Impl>
MakeTestExactlyOnceAckHandler(
    std::unique_ptr<MockExactlyOnceAckHandlerImpl> mock) {
  EXPECT_CALL(*mock, delivery_attempt()).WillRepeatedly(Return(42));
  EXPECT_CALL(*mock, ack_id()).WillRepeatedly(Return(kTestAckId));
  EXPECT_CALL(*mock, subscription()).WillRepeatedly(Return(kTestSubscription));
  auto span = MakeTestSpan();
  opentelemetry::trace::Scope scope(span);
  span->End();
  Span span_holder;
  span_holder.SetSpan(span);
  return MakeTracingExactlyOnceAckHandler(std::move(mock), span_holder);
}

TEST(TracingExactlyOnceAckHandlerTest, AckSuccess) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockExactlyOnceAckHandlerImpl>();
  EXPECT_CALL(*mock, ack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  auto handler = MakeTestExactlyOnceAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler)->ack().get(), StatusIs(StatusCode::kOk));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanHasInstrumentationScope(), SpanKindIsInternal(),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                         SpanNamed("test-subscription ack"))));
}

TEST(TracingExactlyOnceAckHandlerTest, AckError) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockExactlyOnceAckHandlerImpl>();
  EXPECT_CALL(*mock, ack())
      .WillOnce(Return(ByMove(
          make_ready_future(Status{StatusCode::kPermissionDenied, "uh-oh"}))));
  auto handler = MakeTestExactlyOnceAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler)->ack().get(),
              StatusIs(StatusCode::kPermissionDenied, "uh-oh"));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsInternal(),
                     SpanWithStatus(opentelemetry::trace::StatusCode::kError),
                     SpanNamed("test-subscription ack"))));
}

TEST(TracingAckHandlerTest, AckAttributes) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockExactlyOnceAckHandlerImpl>();
  EXPECT_CALL(*mock, ack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  auto handler = MakeTestExactlyOnceAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler)->ack().get(), StatusIs(StatusCode::kOk));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription ack"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kMessagingSystem, "gcp_pubsub")))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription ack"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 "gcp.project_id", "test-project")))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription ack"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kMessagingOperation, "settle")))));
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanNamed("test-subscription ack"),
                         SpanHasAttributes(OTelAttribute<std::string>(
                             sc::kCodeFunction, "pubsub::AckHandler::ack")))));
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

TEST(TracingExactlyOnceAckHandlerTest, NackSuccess) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockExactlyOnceAckHandlerImpl>();
  EXPECT_CALL(*mock, nack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  auto handler = MakeTestExactlyOnceAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler)->nack().get(), StatusIs(StatusCode::kOk));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanHasInstrumentationScope(), SpanKindIsInternal(),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                         SpanNamed("test-subscription nack"))));
}

TEST(TracingExactlyOnceAckHandlerTest, NackError) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockExactlyOnceAckHandlerImpl>();
  EXPECT_CALL(*mock, nack())
      .WillOnce(Return(ByMove(
          make_ready_future(Status{StatusCode::kPermissionDenied, "uh-oh"}))));
  auto handler = MakeTestExactlyOnceAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler)->nack().get(),
              StatusIs(StatusCode::kPermissionDenied, "uh-oh"));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsInternal(),
                     SpanWithStatus(opentelemetry::trace::StatusCode::kError),
                     SpanNamed("test-subscription nack"))));
}

TEST(TracingAckHandlerTest, NackAttributes) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockExactlyOnceAckHandlerImpl>();
  EXPECT_CALL(*mock, nack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  auto handler = MakeTestExactlyOnceAckHandler(std::move(mock));

  EXPECT_THAT(std::move(handler)->nack().get(), StatusIs(StatusCode::kOk));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription nack"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kMessagingSystem, "gcp_pubsub")))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription nack"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 "gcp.project_id", "test-project")))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription nack"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kMessagingOperation, "settle")))));
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanNamed("test-subscription nack"),
                         SpanHasAttributes(OTelAttribute<std::string>(
                             sc::kCodeFunction, "pubsub::AckHandler::nack")))));
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
  auto mock = std::make_unique<MockExactlyOnceAckHandlerImpl>();
  auto handler = MakeTestExactlyOnceAckHandler(std::move(mock));

  EXPECT_EQ(42, handler->delivery_attempt());

  auto spans = span_catcher->GetSpans();
  // The span we created in `MakeTestExactlyOnceAckHandler`.
  EXPECT_THAT(spans, SizeIs(1));
  EXPECT_THAT(spans, Contains(AllOf(SpanNamed("test-subscription subscribe"))));
}

TEST(TracingAckHandlerTest, AckIdNoSpans) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockExactlyOnceAckHandlerImpl>();
  auto handler = MakeTestExactlyOnceAckHandler(std::move(mock));

  EXPECT_EQ(kTestAckId, handler->ack_id());

  auto spans = span_catcher->GetSpans();
  // The span we created in `MakeTestExactlyOnceAckHandler`.
  EXPECT_THAT(spans, SizeIs(1));
  EXPECT_THAT(spans, Contains(AllOf(SpanNamed("test-subscription subscribe"))));
}

TEST(TracingAckHandlerTest, SubscriptionNoSpans) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockExactlyOnceAckHandlerImpl>();
  auto handler = MakeTestExactlyOnceAckHandler(std::move(mock));

  EXPECT_EQ(kTestSubscription, handler->subscription());

  auto spans = span_catcher->GetSpans();
  // The span we created in `MakeTestExactlyOnceAckHandler`.
  EXPECT_THAT(spans, SizeIs(1));
  EXPECT_THAT(spans, Contains(AllOf(SpanNamed("test-subscription subscribe"))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
