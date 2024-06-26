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

#include "google/cloud/pubsub/internal/tracing_batch_callback.h"
#include "google/cloud/pubsub/internal/message_propagator.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/pubsub/testing/mock_batch_callback.h"
#include "google/cloud/pubsub/testing/mock_exactly_once_ack_handler_impl.h"
#include "google/cloud/pubsub/topic.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/trace/scope.h>
#include <opentelemetry/trace/semantic_conventions.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasEvents;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanKindIsConsumer;
using ::google::cloud::testing_util::SpanKindIsInternal;
using ::google::cloud::testing_util::SpanLinksSizeIs;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::NotNull;

namespace {

pubsub::Subscription TestSubscription() {
  return pubsub::Subscription("test-project", "test-sub");
}

std::shared_ptr<BatchCallback> MakeTestBatchCallback(
    std::shared_ptr<BatchCallback> mock) {
  return MakeTracingBatchCallback(std::move(mock),
                                  std::move(TestSubscription()));
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> MakeTestSpan() {
  return internal::GetTracer(internal::CurrentOptions())
      ->StartSpan("test-topic publish");
}

BatchCallback::StreamingPullResponse MakeResponse(int n) {
  std::shared_ptr<opentelemetry::context::propagation::TextMapPropagator>
      propagator = std::make_shared<
          opentelemetry::trace::propagation::HttpTraceContext>();
  BatchCallback::StreamingPullResponse response;
  google::pubsub::v1::StreamingPullResponse r;
  for (int i = 0; i < n; i++) {
    auto span = MakeTestSpan();
    opentelemetry::trace::Scope scope(span);
    auto message = pubsub::MessageBuilder().Build();
    InjectTraceContext(message, *propagator);
    span->End();
    auto proto_message = ToProto(message);
    proto_message.set_message_id("id-" + std::to_string(i));

    auto* m = r.add_received_messages();
    *m->mutable_message() = proto_message;
    m->set_ack_id("ack-id-" + std::to_string(i));
  }
  response.response = std::move(r);
  return response;
}

TEST(TracingBatchCallback, StartAndEndMessage) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);
  auto batch_callback = MakeTestBatchCallback(std::move(mock));

  batch_callback->callback(MakeResponse(1));
  batch_callback->AckEnd("ack-id-0");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                            SpanNamed("test-sub subscribe"),
                            SpanHasEvents(EventNamed("gl-cpp.ack_end")))));
}

TEST(TracingBatchCallback, VerifySpanIsSetInUserCallback) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);
  EXPECT_CALL(*mock, user_callback)
      .WillOnce([&](MessageCallback::MessageAndHandler m) {
        EXPECT_EQ(m.ack_id, "ack-id-0");
        EXPECT_THAT(m.subscribe_span.span, NotNull());
      });
  auto batch_callback = MakeTestBatchCallback(std::move(mock));
  MessageCallback::MessageAndHandler message_and_handler{
      pubsub::MessageBuilder().Build(),
      std::make_unique<pubsub_testing::MockExactlyOnceAckHandlerImpl>(),
      "ack-id-0", Span{}};

  batch_callback->callback(MakeResponse(1));
  batch_callback->user_callback(std::move(message_and_handler));
  batch_callback->AckEnd("ack-id-0");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                            SpanNamed("test-sub subscribe"),
                            SpanHasEvents(EventNamed("gl-cpp.ack_end")))));
}

TEST(TracingBatchCallback, StartAndEndModackSpanForOneMessage) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);
  auto batch_callback = MakeTestBatchCallback(std::move(mock));

  batch_callback->callback(MakeResponse(1));
  google::pubsub::v1::ModifyAckDeadlineRequest request;
  *request.add_ack_ids() = "ack-id-0";
  request.set_ack_deadline_seconds(10);
  request.set_subscription(TestSubscription().FullName());
  auto s = batch_callback->StartModackSpan(request);
  batch_callback->EndModackSpan(s);
  batch_callback->AckEnd("ack-id-0");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("test-sub modack"),
          SpanHasAttributes(
              OTelAttribute<std::string>(sc::kMessagingSystem, "gcp_pubsub"),
              OTelAttribute<std::string>("gcp.project_id", "test-project"),
              OTelAttribute<std::string>(
                  /*sc::kMessagingOperationType=*/"messaging.operation.type",
                  "extend"),
              OTelAttribute<int64_t>(sc::kMessagingBatchMessageCount, 1),
              OTelAttribute<int64_t>(
                  "messaging.gcp_pubsub.message.ack_deadline_seconds", 10),
              OTelAttribute<std::string>(sc::kMessagingDestinationName,
                                         "test-sub")),
          SpanLinksSizeIs(1))));
}

TEST(TracingBatchCallback, StartAndEndModackSpanForMultipleMessages) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);
  auto batch_callback = MakeTestBatchCallback(std::move(mock));

  batch_callback->callback(MakeResponse(2));
  google::pubsub::v1::ModifyAckDeadlineRequest request;
  *request.add_ack_ids() = "ack-id-0";
  *request.add_ack_ids() = "ack-id-1";
  request.set_ack_deadline_seconds(10);
  request.set_subscription(TestSubscription().FullName());
  auto s = batch_callback->StartModackSpan(request);
  batch_callback->EndModackSpan(s);
  batch_callback->AckEnd("ack-id-0");
  batch_callback->AckEnd("ack-id-1");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("test-sub modack"),
          SpanHasAttributes(
              OTelAttribute<std::string>(sc::kMessagingSystem, "gcp_pubsub"),
              OTelAttribute<std::string>("gcp.project_id", "test-project"),
              OTelAttribute<std::string>(
                  /*sc::kMessagingOperationType=*/"messaging.operation.type",
                  "extend"),
              OTelAttribute<int64_t>(sc::kMessagingBatchMessageCount, 2),
              OTelAttribute<int64_t>(
                  "messaging.gcp_pubsub.message.ack_deadline_seconds", 10),
              OTelAttribute<std::string>(sc::kMessagingDestinationName,
                                         "test-sub")),
          SpanLinksSizeIs(2))));
}

TEST(TracingBatchCallback, VerifyModackSpansAreEndedInDestructor) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);

  // Create batch callback within the scope, so it is destroyed before the
  // `GetSpans` call. This will trigger the destructor call and the spans to be
  // ended.
  {
    auto batch_callback = MakeTestBatchCallback(std::move(mock));
    batch_callback->callback(MakeResponse(1));
    google::pubsub::v1::ModifyAckDeadlineRequest request;
    *request.add_ack_ids() = "ack-id-0";
    request.set_ack_deadline_seconds(10);
    request.set_subscription(TestSubscription().FullName());
    batch_callback->StartModackSpan(request);
  }

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsClient(),
                             SpanNamed("test-sub modack"))));
}

TEST(TracingBatchCallback, SubscribeAttributes) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);
  auto batch_callback = MakeTestBatchCallback(std::move(mock));

  batch_callback->callback(MakeResponse(1));
  batch_callback->AckEnd("ack-id-0");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(
          SpanNamed("test-sub subscribe"),
          SpanHasAttributes(
              OTelAttribute<std::string>(sc::kMessagingSystem, "gcp_pubsub"),
              OTelAttribute<std::string>("gcp.project_id", "test-project"),
              OTelAttribute<std::string>(
                  /*sc::kMessagingOperationType=*/"messaging.operation.type",
                  "subscribe"),
              OTelAttribute<std::string>(sc::kMessagingMessageId, "id-0"),
              OTelAttribute<std::string>("messaging.gcp_pubsub.message.ack_id",
                                         "ack-id-0"),
              OTelAttribute<int64_t>("messaging.message.envelope.size", 101),
              OTelAttribute<std::string>(sc::kMessagingDestinationName,
                                         "test-sub")))));
}

TEST(TracingBatchCallback, SubscribeAttributesForOrderingKey) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);
  auto batch_callback = MakeTestBatchCallback(std::move(mock));
  std::shared_ptr<opentelemetry::context::propagation::TextMapPropagator>
      propagator = std::make_shared<
          opentelemetry::trace::propagation::HttpTraceContext>();
  BatchCallback::StreamingPullResponse response;
  google::pubsub::v1::StreamingPullResponse r;
  auto span = MakeTestSpan();
  auto message =
      pubsub::MessageBuilder().SetOrderingKey("ordering-key-0").Build();
  InjectTraceContext(message, *propagator);
  span->End();
  auto proto_message = ToProto(message);
  proto_message.set_message_id("id-0");

  auto* m = r.add_received_messages();
  *m->mutable_message() = proto_message;
  m->set_ack_id("ack-id-0");
  response.response = std::move(r);

  batch_callback->callback(response);
  batch_callback->AckEnd("ack-id-0");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-sub subscribe"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 "messaging.gcp_pubsub.message.ordering_key",
                                 "ordering-key-0")))));
}

TEST(TracingBatchCallback, SubscribeAttributesForExactlyOnce) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);
  auto batch_callback = MakeTestBatchCallback(std::move(mock));
  std::shared_ptr<opentelemetry::context::propagation::TextMapPropagator>
      propagator = std::make_shared<
          opentelemetry::trace::propagation::HttpTraceContext>();
  auto span = MakeTestSpan();
  auto message = pubsub::MessageBuilder().Build();
  InjectTraceContext(message, *propagator);
  span->End();
  auto proto_message = ToProto(message);
  proto_message.set_message_id("id-0");

  google::pubsub::v1::StreamingPullResponse r;
  r.mutable_subscription_properties()->set_exactly_once_delivery_enabled(true);
  auto* m = r.add_received_messages();
  *m->mutable_message() = proto_message;
  m->set_ack_id("ack-id-0");
  BatchCallback::StreamingPullResponse response;
  response.response = std::move(r);

  batch_callback->callback(response);
  batch_callback->AckEnd("ack-id-0");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(AllOf(
                  SpanNamed("test-sub subscribe"),
                  SpanHasAttributes(OTelAttribute<bool>(
                      "messaging.gcp_pubsub.subscription.exactly_once_delivery",
                      true)))));
}

TEST(TracingBatchCallback, StartAndEndConcurrencyControlSpan) {
  namespace sc = opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);
  auto batch_callback = MakeTestBatchCallback(std::move(mock));

  batch_callback->callback(MakeResponse(1));
  batch_callback->StartConcurrencyControl("ack-id-0");
  batch_callback->EndConcurrencyControl("ack-id-0");
  batch_callback->AckEnd("ack-id-0");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsInternal(),
                            SpanNamed("subscriber concurrency control"),
                            SpanHasAttributes(OTelAttribute<std::string>(
                                sc::kMessagingSystem, "gcp_pubsub")))));
}

TEST(TracingBatchCallback, StartAndEndSchedulerSpan) {
  namespace sc = opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);
  auto batch_callback = MakeTestBatchCallback(std::move(mock));

  batch_callback->callback(MakeResponse(1));
  batch_callback->StartScheduler("ack-id-0");
  batch_callback->EndScheduler("ack-id-0");
  batch_callback->AckEnd("ack-id-0");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsInternal(),
                            SpanNamed("subscriber scheduler"),
                            SpanHasAttributes(OTelAttribute<std::string>(
                                sc::kMessagingSystem, "gcp_pubsub")))));
}

TEST(TracingBatchCallback, VerifyDestructorEndsAllSpans) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);
  // Create batch callback within the scope, so it is destroyed before the
  // `GetSpans` call. This will trigger the destructor call and the spans to be
  // ended.
  {
    auto batch_callback = MakeTestBatchCallback(std::move(mock));
    batch_callback->callback(MakeResponse(1));
  }
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                            SpanNamed("test-sub subscribe"))));
}

TEST(TracingBatchCallback, StartAndEndMultipleMessage) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);
  auto batch_callback = MakeTestBatchCallback(std::move(mock));

  batch_callback->callback(MakeResponse(3));
  batch_callback->AckEnd("ack-id-0");
  batch_callback->AckEnd("ack-id-1");
  batch_callback->AckEnd("ack-id-2");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                            SpanNamed("test-sub subscribe"),
                            SpanHasEvents(EventNamed("gl-cpp.ack_end")))));
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                            SpanNamed("test-sub subscribe"),
                            SpanHasEvents(EventNamed("gl-cpp.ack_end")))));
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                            SpanNamed("test-sub subscribe"),
                            SpanHasEvents(EventNamed("gl-cpp.ack_end")))));
}

TEST(TracingBatchCallback, Ack) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);
  auto batch_callback = MakeTestBatchCallback(std::move(mock));

  batch_callback->callback(MakeResponse(1));
  batch_callback->AckStart("ack-id-0");
  batch_callback->AckEnd("ack-id-0");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                            SpanNamed("test-sub subscribe"),
                            SpanHasAttributes(OTelAttribute<std::string>(
                                "messaging.gcp_pubsub.result", "ack")),
                            SpanHasEvents(EventNamed("gl-cpp.ack_start"),
                                          EventNamed("gl-cpp.ack_end")))));
}

TEST(TracingBatchCallback, Nack) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);
  auto batch_callback = MakeTestBatchCallback(std::move(mock));

  batch_callback->callback(MakeResponse(1));
  batch_callback->NackStart("ack-id-0");
  batch_callback->NackEnd("ack-id-0");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                            SpanNamed("test-sub subscribe"),
                            SpanHasAttributes(OTelAttribute<std::string>(
                                "messaging.gcp_pubsub.result", "nack")),
                            SpanHasEvents(EventNamed("gl-cpp.nack_start"),
                                          EventNamed("gl-cpp.nack_end")))));
}

TEST(TracingBatchCallback, Modack) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);
  auto batch_callback = MakeTestBatchCallback(std::move(mock));

  batch_callback->callback(MakeResponse(1));
  batch_callback->ModackStart("ack-id-0");
  batch_callback->ModackEnd("ack-id-0");
  batch_callback->AckEnd("ack-id-0");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                            SpanNamed("test-sub subscribe"),
                            SpanHasEvents(EventNamed("gl-cpp.modack_start"),
                                          EventNamed("gl-cpp.modack_end"),
                                          EventNamed("gl-cpp.ack_end")))));
}

TEST(TracingBatchCallback, Expire) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);

  // This needs to be in its own scope so the span is ended.
  {
    auto batch_callback = MakeTestBatchCallback(std::move(mock));
    batch_callback->callback(MakeResponse(1));
    batch_callback->ExpireMessage("ack-id-0");
  }

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                            SpanNamed("test-sub subscribe"),
                            SpanHasAttributes(OTelAttribute<std::string>(
                                "messaging.gcp_pubsub.result", "expired")),
                            SpanHasEvents(EventNamed("gl-cpp.expired")))));
}

TEST(TracingBatchCallback, ExpireThenNack) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);
  auto batch_callback = MakeTestBatchCallback(std::move(mock));

  batch_callback->callback(MakeResponse(1));
  batch_callback->ExpireMessage("ack-id-0");
  batch_callback->NackEnd("ack-id-0");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                            SpanNamed("test-sub subscribe"),
                            SpanHasAttributes(OTelAttribute<std::string>(
                                "messaging.gcp_pubsub.result", "nack")),
                            SpanHasEvents(EventNamed("gl-cpp.nack_end")))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
