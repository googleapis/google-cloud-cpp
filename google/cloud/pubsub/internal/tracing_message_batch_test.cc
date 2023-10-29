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
using ::google::cloud::testing_util::OTelContextCaptured;
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

/// Saves a message for each span and ends @p spans. If @p end_spans is true, it
/// will end the spans.
void SaveMessages(
    std::vector<opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>>
        spans,
   std::shared_ptr<MessageBatch> const& message_batch, bool end_spans = true) {
  for (size_t i = 0; i < spans.size(); i++) {
    auto message =
        pubsub::MessageBuilder().SetData("test" + std::to_string(i)).Build();
    auto scope = opentelemetry::trace::Scope(spans[i]);
    message_batch->SaveMessage(message);
    if (end_spans) {
      spans[i]->End();
    }
  }
}

TEST(TracingMessageBatch, SaveMessageAddsEvent) {
  auto span_catcher = InstallSpanCatcher();
  auto span = MakeSpan("test span");
  opentelemetry::trace::Scope scope(span);
  auto mock = std::make_shared<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, SaveMessage(_));
  auto message_batch = MakeTracingMessageBatch(std::move(mock));
  auto message = pubsub::MessageBuilder().SetData("test").Build();

  message_batch->SaveMessage(message);

  span->End();

  EXPECT_THAT(
      span_catcher->GetSpans(),
      Contains(AllOf(SpanHasEvents(EventNamed("gl-cpp.added_to_batch")))));
}

TEST(TracingMessageBatch, Flush) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto message_span = MakeSpan("test span");
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, SaveMessage(_));
  EXPECT_CALL(*mock, Flush).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    return [](auto) { EXPECT_FALSE(OTelContextCaptured()); };
  });
  auto message_batch = MakeTracingMessageBatch(std::move(mock));
  auto initial_spans = {message_span};
  SaveMessages(initial_spans, message_batch);

  auto end_spans = message_batch->Flush();
  end_spans(make_ready_future());

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
  auto mock = std::make_shared<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, SaveMessage(_)).Times(2);
  EXPECT_CALL(*mock, Flush).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return [](auto) {};
  });
  auto message_batch = MakeTracingMessageBatch(std::move(mock));
  auto initial_spans = {message_span1, message_span2};
  SaveMessages(initial_spans, message_batch);

  auto end_spans = message_batch->Flush();
  end_spans(make_ready_future());

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
  auto mock = std::make_unique<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, SaveMessage(_)).Times(kBatchSize);
  EXPECT_CALL(*mock, Flush).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return [](auto) {};
  });
  auto message_batch = MakeTracingMessageBatch(std::move(mock));
  SaveMessages(CreateSpans(kBatchSize), message_batch);
  // Only catch the batch sink spans.
  auto span_catcher = InstallSpanCatcher();

  auto end_spans = message_batch->Flush();
  end_spans(make_ready_future());

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
  auto mock = std::make_shared<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, SaveMessage(_)).Times(kBatchSize);
  EXPECT_CALL(*mock, Flush).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return [](auto) {};
  });
  auto message_batch = MakeTracingMessageBatch(std::move(mock));
  SaveMessages(CreateSpans(kBatchSize), message_batch);
  // Only catch the batch sink spans.
  auto span_catcher = InstallSpanCatcher();

  auto end_spans = message_batch->Flush();
  end_spans(make_ready_future());

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
  auto mock = std::make_unique<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, SaveMessage(_));
  EXPECT_CALL(*mock, Flush).WillOnce([] { return [](auto) {}; });
  auto message_batch = MakeTracingMessageBatch(std::move(mock));
  auto message_spans = CreateSpans(1);
  SaveMessages(message_spans, message_batch, /*end_spans=*/false);

  auto end_spans = message_batch->Flush();
  end_spans(make_ready_future());

  EndSpans(message_spans);

  EXPECT_THAT(
      span_catcher->GetSpans(),
      Contains(AllOf(
          SpanNamed("test span 0"),
          SpanHasAttributes(
              OTelAttribute<std::string>("pubsub.batch_sink.trace_id", _),
              OTelAttribute<std::string>("pubsub.batch_sink.span_id", _)))));
}

TEST(TracingMessageBatch, FlushSpanAddsEvent) {
  // The span catcher must be installed before the message span is created.
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, Flush).WillOnce([] { return [](auto) {}; });
  EXPECT_CALL(*mock, SaveMessage(_));
  auto message_batch = MakeTracingMessageBatch(std::move(mock));
  auto message_spans = CreateSpans(1);
  SaveMessages(message_spans, message_batch, /*end_spans=*/false);

  auto end_spans = message_batch->Flush();
  end_spans(make_ready_future());

  EndSpans(message_spans);

  EXPECT_THAT(
      span_catcher->GetSpans(),
      Contains(AllOf(SpanNamed("test span 0"),
                     SpanHasEvents(
                                   EventNamed("gl-cpp.batch_flushed")))));
}

TEST(TracingMessageBatch, FlushAddsEventForMultipleMessages) {
  // The span catcher must be installed before the message span is created.
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, Flush).WillOnce([] { return [](auto) {}; });
  EXPECT_CALL(*mock, SaveMessage(_)).Times(2);
  auto message_batch = MakeTracingMessageBatch(std::move(mock));
  auto message_spans = CreateSpans(2);
  SaveMessages(message_spans, message_batch, /*end_spans=*/false);

  auto end_spans = message_batch->Flush();
  end_spans(make_ready_future());

  EndSpans(message_spans);
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanNamed("test span 0"),
                         SpanHasEvents(EventNamed("gl-cpp.batch_flushed")))));
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanNamed("test span 1"),
                         SpanHasEvents(EventNamed("gl-cpp.batch_flushed")))));
}

// TODO(#12528): Add an end to end test.

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
