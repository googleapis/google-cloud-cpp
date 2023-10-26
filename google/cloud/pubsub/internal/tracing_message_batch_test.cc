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

#include "google/cloud/pubsub/internal/tracing_message_batch.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/testing/mock_message_batch.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <opentelemetry/trace/semantic_conventions.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::internal::MakeSpan;
using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::LinkHasSpanContext;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasEvents;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanLinkAttributesAre;
using ::google::cloud::testing_util::SpanLinksSizeIs;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace {

void EndSpans(std::vector<opentelemetry::nostd::shared_ptr<
                  opentelemetry::trace::Span>> const& spans) {
  for (auto const& span : spans) {
    span->End();
  }
}

/// Creates @p n spans.
auto CreateSpans(int n) {
  std::vector<opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>>
      spans(n);
  std::generate(spans.begin(), spans.end(), [i = 0]() mutable {
    return MakeSpan("test span " + std::to_string(i++));
  });
  return spans;
}

TEST(TracingMessageBatch, SaveMessage) {
  auto span = MakeSpan("test span");
  opentelemetry::trace::Scope scope(span);
  auto mock = std::make_unique<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, SaveMessage);
  auto message_batch = std::make_unique<TracingMessageBatch>(std::move(mock));
  auto message = pubsub::MessageBuilder().SetData("test").Build();

  message_batch->SaveMessage(message);

  span->End();
  auto spans = message_batch->GetMessageSpans();
  EXPECT_THAT(spans, ElementsAre(span));
}

TEST(TracingMessageBatch, SaveMultipleMessages) {
  auto mock = std::make_unique<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, SaveMessage).Times(2);
  auto message_batch = std::make_unique<TracingMessageBatch>(std::move(mock));
  auto message = pubsub::MessageBuilder().SetData("test").Build();

  // Save the first span.
  auto span1 = MakeSpan("test span");
  opentelemetry::trace::Scope scope1(span1);
  message_batch->SaveMessage(message);
  span1->End();
  // Save the second span.
  auto span2 = MakeSpan("test span");
  opentelemetry::trace::Scope scope2(span2);
  message_batch->SaveMessage(message);
  span2->End();

  auto spans = message_batch->GetMessageSpans();
  EXPECT_THAT(spans, ElementsAre(span1, span2));
}

TEST(TracingMessageBatch, SaveMessageAddsEvent) {
  auto span_catcher = InstallSpanCatcher();
  auto span = MakeSpan("test span");
  opentelemetry::trace::Scope scope(span);
  auto mock = std::make_unique<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, SaveMessage);
  auto message_batch = std::make_unique<TracingMessageBatch>(std::move(mock));
  auto message = pubsub::MessageBuilder().SetData("test").Build();

  message_batch->SaveMessage(message);

  span->End();
  auto spans = message_batch->GetMessageSpans();
  EXPECT_THAT(spans, ElementsAre(span));

  EXPECT_THAT(
      span_catcher->GetSpans(),
      Contains(AllOf(SpanHasEvents(EventNamed("gl-cpp.added_to_batch")))));
}

TEST(TracingMessageBatch, Flush) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto message_span = MakeSpan("test span");
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, Flush).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return [](auto) {};
  });
  auto initial_spans = {message_span};
  auto message_batch =
      std::make_unique<TracingMessageBatch>(std::move(mock), initial_spans);

  auto end_spans = message_batch->Flush();
  end_spans(make_ready_future());

  EXPECT_THAT(message_batch->GetMessageSpans(), IsEmpty());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsClient(),
                             SpanNamed("BatchSink::AsyncPublish"),
                             SpanHasAttributes(OTelAttribute<std::int64_t>(
                                 sc::kMessagingBatchMessageCount, 1)),
                             SpanHasLinks(AllOf(
                                 LinkHasSpanContext(message_span->GetContext()),
                                 SpanLinkAttributesAre(OTelAttribute<int64_t>(
                                     "messaging.pubsub.message.link", 0)))))));
}

TEST(TracingMessageBatch, FlushSmallBatch) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto message_span1 = MakeSpan("test span 1");
  auto message_span2 = MakeSpan("test span 2");
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, Flush).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return [](auto) {};
  });
  auto initial_spans = {message_span1, message_span2};
  auto message_batch =
      std::make_unique<TracingMessageBatch>(std::move(mock), initial_spans);

  auto end_spans = message_batch->Flush();
  end_spans(make_ready_future());

  EXPECT_THAT(message_batch->GetMessageSpans(), IsEmpty());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("BatchSink::AsyncPublish"),
          SpanHasAttributes(
              OTelAttribute<std::int64_t>(sc::kMessagingBatchMessageCount, 2)),
          SpanHasLinks(AllOf(LinkHasSpanContext(message_span1->GetContext()),
                             SpanLinkAttributesAre(OTelAttribute<int64_t>(
                                 "messaging.pubsub.message.link", 0))),
                       AllOf(LinkHasSpanContext(message_span2->GetContext()),
                             SpanLinkAttributesAre(OTelAttribute<int64_t>(
                                 "messaging.pubsub.message.link", 1)))))));
}

TEST(TracingMessageBatch, FlushBatchWithOtelLimit) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto constexpr kBatchSize = 128;
  auto initial_spans = CreateSpans(kBatchSize);
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, Flush).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return [](auto) {};
  });
  auto message_batch =
      std::make_unique<TracingMessageBatch>(std::move(mock), initial_spans);

  auto end_spans = message_batch->Flush();
  end_spans(make_ready_future());

  EXPECT_THAT(message_batch->GetMessageSpans(), IsEmpty());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsClient(),
                             SpanNamed("BatchSink::AsyncPublish"),
                             SpanHasAttributes(OTelAttribute<std::int64_t>(
                                 sc::kMessagingBatchMessageCount, 128)),
                             SpanLinksSizeIs(128))));
}

TEST(TracingMessageBatch, FlushLargeBatch) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto constexpr kBatchSize = 129;
  auto initial_spans = CreateSpans(kBatchSize);
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, Flush).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return [](auto) {};
  });
  auto message_batch =
      std::make_unique<TracingMessageBatch>(std::move(mock), initial_spans);

  auto end_spans = message_batch->Flush();
  end_spans(make_ready_future());

  EXPECT_THAT(message_batch->GetMessageSpans(), IsEmpty());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanNamed("BatchSink::AsyncPublish"),
                         SpanHasAttributes(OTelAttribute<std::int64_t>(
                             sc::kMessagingBatchMessageCount, kBatchSize)))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("BatchSink::AsyncPublish - Batch #0"),
                             SpanLinksSizeIs(128))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("BatchSink::AsyncPublish - Batch #1"),
                             SpanLinksSizeIs(1))));
}

TEST(TracingMessageBatch, FlushAddsSpanIdAndTraceIdAttribute) {
  // The span catcher must be installed before the message span is created.
  auto span_catcher = InstallSpanCatcher();
  auto message_span = MakeSpan("test message span");
  auto mock = std::make_unique<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, Flush).WillOnce([] { return [](auto) {}; });
  auto initial_spans = {message_span};
  auto message_batch =
      std::make_unique<TracingMessageBatch>(std::move(mock), initial_spans);

  auto end_spans = message_batch->Flush();
  end_spans(make_ready_future());

  // The span must end before it can be processed by the span catcher.
  EndSpans(initial_spans);

  EXPECT_THAT(
      span_catcher->GetSpans(),
      Contains(AllOf(
          SpanNamed("test message span"),
          SpanHasAttributes(
              OTelAttribute<std::string>("pubsub.batch_sink.trace_id", _),
              OTelAttribute<std::string>("pubsub.batch_sink.span_id", _)))));
}

TEST(TracingMessageBatch, FlushSpanMetadataAddsEvent) {
  // The span catcher must be installed before the message span is created.
  auto span_catcher = InstallSpanCatcher();
  auto message_span = MakeSpan("test message span");
  auto mock = std::make_unique<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, Flush).WillOnce([] { return [](auto) {}; });
  auto initial_spans = {message_span};
  auto message_batch =
      std::make_unique<TracingMessageBatch>(std::move(mock), initial_spans);

  auto end_spans = message_batch->Flush();
  end_spans(make_ready_future());

  // The span must end before it can be processed by the span catcher.
  EndSpans(initial_spans);

  EXPECT_THAT(
      span_catcher->GetSpans(),
      Contains(AllOf(SpanNamed("test message span"),
                     SpanHasEvents(EventNamed("gl-cpp.batch_flushed")))));
}

TEST(TracingMessageBatch, FlushAddsEventForMultipleMessages) {
  // The span catcher must be installed before the message span is created.
  auto span_catcher = InstallSpanCatcher();
  auto span1 = MakeSpan("test message span1");
  auto span2 = MakeSpan("test message span2");
  auto mock = std::make_unique<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, Flush).WillOnce([] { return [](auto) {}; });
  auto initial_spans = {span1, span2};
  auto message_batch =
      std::make_unique<TracingMessageBatch>(std::move(mock), initial_spans);

  auto end_spans = message_batch->Flush();
  end_spans(make_ready_future());

  // The span must end before it can be processed by the span catcher.
  EndSpans(initial_spans);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanNamed("test message span1"),
                         SpanHasEvents(EventNamed("gl-cpp.batch_flushed")))));
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanNamed("test message span2"),
                         SpanHasEvents(EventNamed("gl-cpp.batch_flushed")))));
}

// TODO(#12528): Add an end to end test.

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
