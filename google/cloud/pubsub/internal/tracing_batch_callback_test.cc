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
#include "google/cloud/pubsub/topic.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "opentelemetry/context/propagation/text_map_propagator.h"
#include "opentelemetry/trace/scope.h"
#include <gmock/gmock.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/trace/semantic_conventions.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::SpanHasEvents;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsConsumer;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::AllOf;
using ::testing::Contains;

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
  batch_callback->EndMessage("ack-id-0", "test-event");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                            SpanNamed("test-sub subscribe"),
                            SpanHasEvents(EventNamed("test-event")))));
}

TEST(TracingBatchCallback, VerifyDestructorEndsAllSpans) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);
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
  batch_callback->EndMessage("ack-id-0", "test-event-0");
  batch_callback->EndMessage("ack-id-1", "test-event-1");
  batch_callback->EndMessage("ack-id-2", "test-event-2");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                            SpanNamed("test-sub subscribe"),
                            SpanHasEvents(EventNamed("test-event-0")))));
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                            SpanNamed("test-sub subscribe"),
                            SpanHasEvents(EventNamed("test-event-1")))));
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                            SpanNamed("test-sub subscribe"),
                            SpanHasEvents(EventNamed("test-event-2")))));
}

TEST(TracingBatchCallback, AddEvent) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);
  auto batch_callback = MakeTestBatchCallback(std::move(mock));
  {
    batch_callback->callback(MakeResponse(1));
    batch_callback->AddEvent("ack-id-0", "test-added-event");
  }

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                            SpanNamed("test-sub subscribe"),
                            SpanHasEvents(EventNamed("test-added-event")))));
}

TEST(TracingBatchCallback, DoesNotAddEvent) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock, callback).Times(1);
  auto batch_callback = MakeTestBatchCallback(std::move(mock));

  batch_callback->callback(MakeResponse(1));
  batch_callback->AddEvent("ack-id-10", "test-added-event");
  batch_callback->EndMessage("ack-id-0", "test-event");

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                            SpanNamed("test-sub subscribe"),
                            SpanHasEvents(EventNamed("test-event")))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
