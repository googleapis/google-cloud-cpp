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

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::internal::MakeSpan;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::SizeIs;

namespace {

void EndSpans(std::vector<opentelemetry::nostd::shared_ptr<
                  opentelemetry::trace::Span>> const& spans) {
  for (auto const& span : spans) {
    span->End();
  }
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

TEST(TracingMessageBatch, Flush) {
  // namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto message_span = MakeSpan("test span");
  auto span_catcher = InstallSpanCatcher();
  opentelemetry::trace::Scope scope(message_span);
  auto mock = std::make_unique<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, Flush).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
  });
  auto initial_spans = {message_span};
  auto message_batch =
      std::make_unique<TracingMessageBatch>(std::move(mock), initial_spans);

  message_batch->Flush();

  message_span->End();
  // The span must end before it can be processed by the span catcher.
  EndSpans(message_batch->GetBatchSinkSpans());

  EXPECT_THAT(message_batch->GetMessageSpans(), IsEmpty());
  EXPECT_THAT(message_batch->GetBatchSinkSpans(), SizeIs(1));
  EXPECT_THAT(
      span_catcher->GetSpans(),
      Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsClient(),
                     SpanNamed("BatchSink::AsyncPublish"),
                     SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                     SpanHasAttributes(OTelAttribute<std::int64_t>(
                         "messaging.pubsub.num_messages_in_batch", 1)))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
