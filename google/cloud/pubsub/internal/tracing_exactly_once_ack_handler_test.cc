// Copyright 2024 Google LLC
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
using ::google::cloud::testing_util::IsActive;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::OTelContextCaptured;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsInternal;
using ::google::cloud::testing_util::SpanLinksSizeIs;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Not;
using ::testing::Return;

pubsub::Subscription const kTestSubscription =
    pubsub::Subscription("test-project", "test-subscription");
auto constexpr kTestAckId = "test-ack-id";

std::unique_ptr<pubsub::ExactlyOnceAckHandler::Impl>
MakeTestExactlyOnceAckHandler(
    std::unique_ptr<MockExactlyOnceAckHandlerImpl> mock) {
  EXPECT_CALL(*mock, delivery_attempt()).WillRepeatedly(Return(42));
  EXPECT_CALL(*mock, ack_id()).WillRepeatedly(Return(kTestAckId));
  EXPECT_CALL(*mock, subscription()).WillRepeatedly(Return(kTestSubscription));
  return MakeTracingExactlyOnceAckHandler(
      std::move(mock), {internal::MakeSpan("test-subscription subscribe")});
}

TEST(TracingExactlyOnceAckHandlerTest, AckSuccess) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockExactlyOnceAckHandlerImpl>();
  EXPECT_CALL(*mock, ack()).WillOnce([&]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    return make_ready_future(Status{});
  });
  auto handler = MakeTestExactlyOnceAckHandler(std::move(mock));

  EXPECT_STATUS_OK(std::move(handler)->ack().get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              ElementsAre(AllOf(
                  SpanHasInstrumentationScope(), SpanKindIsInternal(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanNamed("test-subscription ack"), SpanLinksSizeIs(1))));
}

TEST(TracingExactlyOnceAckHandlerTest, AckSuccessWithNoSubscribeSpan) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockExactlyOnceAckHandlerImpl>();
  EXPECT_CALL(*mock, ack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  EXPECT_CALL(*mock, delivery_attempt()).WillRepeatedly(Return(42));
  EXPECT_CALL(*mock, ack_id()).WillRepeatedly(Return(kTestAckId));
  EXPECT_CALL(*mock, subscription()).WillRepeatedly(Return(kTestSubscription));
  auto handler = MakeTracingExactlyOnceAckHandler(std::move(mock), {});

  EXPECT_STATUS_OK(std::move(handler)->ack().get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanHasInstrumentationScope(), SpanKindIsInternal(),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                         SpanNamed("test-subscription ack"))));
}

TEST(TracingExactlyOnceAckHandlerTest, AckActiveSpan) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockExactlyOnceAckHandlerImpl>();
  auto sub_span = internal::MakeSpan("test-subscription subscribe");
  EXPECT_CALL(*mock, ack()).WillOnce([sub_span] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    EXPECT_THAT(sub_span, Not(IsActive()));
    return make_ready_future(Status{});
  });
  EXPECT_CALL(*mock, delivery_attempt()).WillRepeatedly(Return(42));
  EXPECT_CALL(*mock, ack_id()).WillRepeatedly(Return(kTestAckId));
  EXPECT_CALL(*mock, subscription()).WillRepeatedly(Return(kTestSubscription));
  auto handler = MakeTracingExactlyOnceAckHandler(std::move(mock), {sub_span});

  EXPECT_STATUS_OK(std::move(handler)->ack().get());

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
  EXPECT_THAT(spans,
              ElementsAre(AllOf(
                  SpanHasInstrumentationScope(), SpanKindIsInternal(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError),
                  SpanNamed("test-subscription ack"), SpanLinksSizeIs(1))));
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
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("test-subscription ack"),
          SpanHasAttributes(
              OTelAttribute<std::string>(sc::kMessagingSystem, "gcp_pubsub"),
              OTelAttribute<std::string>("gcp.project_id", "test-project"),
              OTelAttribute<std::string>(sc::kMessagingOperation, "settle"),
              OTelAttribute<std::string>(sc::kCodeFunction,
                                         "pubsub::AckHandler::ack"),
              OTelAttribute<std::int32_t>(
                  "messaging.gcp_pubsub.message.delivery_attempt", 42),
              OTelAttribute<std::string>(sc::kMessagingDestinationName,
                                         "test-subscription")))));
}

TEST(TracingExactlyOnceAckHandlerTest, NackSuccess) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockExactlyOnceAckHandlerImpl>();
  EXPECT_CALL(*mock, nack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  auto handler = MakeTestExactlyOnceAckHandler(std::move(mock));

  EXPECT_STATUS_OK(std::move(handler)->nack().get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsInternal(),
                     SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                     SpanNamed("test-subscription nack"), SpanLinksSizeIs(1))));
}

TEST(TracingExactlyOnceAckHandlerTest, NackActiveSpan) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockExactlyOnceAckHandlerImpl>();
  auto sub_span = internal::MakeSpan("test-subscription subscribe");
  EXPECT_CALL(*mock, nack()).WillOnce([sub_span] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    EXPECT_THAT(sub_span, Not(IsActive()));
    return make_ready_future(Status{});
  });
  EXPECT_CALL(*mock, delivery_attempt()).WillRepeatedly(Return(42));
  EXPECT_CALL(*mock, ack_id()).WillRepeatedly(Return(kTestAckId));
  EXPECT_CALL(*mock, subscription()).WillRepeatedly(Return(kTestSubscription));
  auto handler = MakeTracingExactlyOnceAckHandler(std::move(mock), {sub_span});

  EXPECT_STATUS_OK(std::move(handler)->nack().get());

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
                     SpanNamed("test-subscription nack"), SpanLinksSizeIs(1))));
}

TEST(TracingExactlyOnceAckHandlerTest, NackSuccessWithNoSubscribeSpan) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockExactlyOnceAckHandlerImpl>();
  EXPECT_CALL(*mock, nack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  EXPECT_CALL(*mock, delivery_attempt()).WillRepeatedly(Return(42));
  EXPECT_CALL(*mock, ack_id()).WillRepeatedly(Return(kTestAckId));
  EXPECT_CALL(*mock, subscription()).WillRepeatedly(Return(kTestSubscription));
  auto handler = MakeTracingExactlyOnceAckHandler(std::move(mock), {});

  EXPECT_STATUS_OK(std::move(handler)->nack().get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanHasInstrumentationScope(), SpanKindIsInternal(),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
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
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("test-subscription nack"),
          SpanHasAttributes(
              OTelAttribute<std::string>(sc::kMessagingSystem, "gcp_pubsub"),
              OTelAttribute<std::string>("gcp.project_id", "test-project"),
              OTelAttribute<std::string>(sc::kMessagingOperation, "settle"),
              OTelAttribute<std::string>(sc::kCodeFunction,
                                         "pubsub::AckHandler::nack"),
              OTelAttribute<std::int32_t>(
                  "messaging.gcp_pubsub.message.delivery_attempt", 42),
              OTelAttribute<std::string>(sc::kMessagingDestinationName,
                                         "test-subscription")))));
}

TEST(TracingAckHandlerTest, DeliveryAttempt) {
  auto mock = std::make_unique<MockExactlyOnceAckHandlerImpl>();
  EXPECT_CALL(*mock, delivery_attempt()).WillOnce(Return(42));
  auto handler = MakeTracingExactlyOnceAckHandler(std::move(mock), {});

  EXPECT_EQ(42, handler->delivery_attempt());
}

TEST(TracingAckHandlerTest, AckId) {
  auto mock = std::make_unique<MockExactlyOnceAckHandlerImpl>();
  EXPECT_CALL(*mock, ack_id()).WillOnce(Return(kTestAckId));
  auto handler = MakeTracingExactlyOnceAckHandler(std::move(mock), {});

  EXPECT_EQ(kTestAckId, handler->ack_id());
}

TEST(TracingAckHandlerTest, Subscription) {
  auto mock = std::make_unique<MockExactlyOnceAckHandlerImpl>();
  EXPECT_CALL(*mock, subscription()).WillOnce(Return(kTestSubscription));
  auto handler = MakeTracingExactlyOnceAckHandler(std::move(mock), {});

  EXPECT_EQ(kTestSubscription, handler->subscription());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
