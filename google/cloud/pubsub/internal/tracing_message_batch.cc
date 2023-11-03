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

#include "google/cloud/pubsub/internal/tracing_message_batch.h"
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/pubsub/internal/publisher_stub.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/opentelemetry.h"
#include "opentelemetry/context/runtime_context.h"
#include "opentelemetry/trace/context.h"
#include "opentelemetry/trace/semantic_conventions.h"
#include "opentelemetry/trace/span.h"
#include <algorithm>
#include <string>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace {
using Spans =
    std::vector<opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>>;
using Attributes =
    std::vector<std::pair<opentelemetry::nostd::string_view,
                          opentelemetry::common::AttributeValue>>;
using Links =
    std::vector<std::pair<opentelemetry::trace::SpanContext, Attributes>>;

/// Creates a link for each span in the range @p begin to @p end.
auto MakeLinks(Spans::const_iterator begin, Spans::const_iterator end) {
  Links links;
  std::transform(begin, end, std::back_inserter(links),
                 [i = static_cast<std::int64_t>(0)](auto const& span) mutable {
                   return std::make_pair(
                       span->GetContext(),
                       Attributes{{"messaging.pubsub.message.link", i++}});
                 });
  return links;
}

auto MakeParent(Links const& links, Spans const& message_spans) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  // Detach current otel context before creating the span.
  internal::DetachOTelContext(
      opentelemetry::context::RuntimeContext::GetCurrent());
  auto batch_sink_parent =
      internal::MakeSpan("BatchSink::AsyncPublish",
                         /*attributes=*/
                         {{sc::kMessagingBatchMessageCount,
                           static_cast<std::int64_t>(message_spans.size())}},
                         /*links*/ std::move(links));

  // Add metadata to the message spans about the batch sink span.
  auto context = batch_sink_parent->GetContext();
  auto trace_id = internal::ToString(context.trace_id());
  auto span_id = internal::ToString(context.span_id());
  for (auto const& message_span : message_spans) {
    message_span->AddEvent("gl-cpp.batch_flushed");
    message_span->SetAttribute("pubsub.batch_sink.trace_id", trace_id);
    message_span->SetAttribute("pubsub.batch_sink.span_id", span_id);
  }
  return batch_sink_parent;
}

auto MakeChild(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> const& parent,
    int count, Links const& links) {
  opentelemetry::trace::StartSpanOptions options;
  options.parent = parent->GetContext();
  return internal::MakeSpan(
      "BatchSink::AsyncPublish - Batch #" + std::to_string(count),
      /*attributes=*/{{}},
      /*links=*/links, options);
}

Spans MakeBatchSinkSpans(Spans message_spans) {
  int64_t max_otel_links = 128;
  Spans batch_sink_spans;
  // If the batch size is less than the max size, add the links to a single
  // span. If the batch size is greater than the max size, create a parent
  // span with no links and each child spans will contain links.
  if (static_cast<int64_t>(message_spans.size()) <= max_otel_links) {
    batch_sink_spans.push_back(MakeParent(
        MakeLinks(message_spans.begin(), message_spans.end()), message_spans));
    return batch_sink_spans;
  }
  batch_sink_spans.push_back(MakeParent({{}}, message_spans));
  auto batch_sink_parent = batch_sink_spans.front();

  auto cut = [&message_spans, max_otel_links](auto i) {
    auto const batch_size = static_cast<std::ptrdiff_t>(max_otel_links);
    return std::next(
        i, std::min(batch_size, std::distance(i, message_spans.end())));
  };
  int count = 0;
  for (auto i = message_spans.begin(); i != message_spans.end(); i = cut(i)) {
    // Generates child spans with links between [i, min(i + batch_size, end))
    // such that each child span will have exactly batch_size elements or less.
    batch_sink_spans.push_back(
        MakeChild(batch_sink_parent, count++, MakeLinks(i, cut(i))));
  }

  return batch_sink_spans;
}

}  // namespace

/**
 * Records spans related to a batch messages across calls and
 * callbacks in the `BatchingPublisherConnection`.
 */
class TracingMessageBatch : public MessageBatch {
 public:
  explicit TracingMessageBatch(std::shared_ptr<MessageBatch> child)
      : child_(std::move(child)) {}

  ~TracingMessageBatch() override = default;

  void SaveMessage(pubsub::Message m) override {
    auto active_span = opentelemetry::trace::GetSpan(
        opentelemetry::context::RuntimeContext::GetCurrent());
    active_span->AddEvent("gl-cpp.added_to_batch");
    {
      std::lock_guard<std::mutex> lk(mu_);
      message_spans_.push_back(std::move(active_span));
    }
    child_->SaveMessage(std::move(m));
  }

  std::function<void(future<void>)> Flush() override {
    decltype(message_spans_) message_spans;
    {
      std::lock_guard<std::mutex> lk(mu_);
      message_spans.swap(message_spans_);
    }

    auto batch_sink_spans = MakeBatchSinkSpans(std::move(message_spans));

    // The first span in `batch_sink_spans` is the parent to the other spans in
    // the vector.
    auto scope =
        std::make_shared<internal::OTelScope>(batch_sink_spans.front());
    // Capture the scope so it stays alive the returned function
    // is called.
    return [scope = std::move(scope),
            oc = opentelemetry::context::RuntimeContext::GetCurrent(),
            next = child_->Flush(),
            spans = std::move(batch_sink_spans)](auto f) mutable {
      for (auto& span : spans) {
        internal::EndSpan(*span);
      }
      internal::DetachOTelContext(oc);
      next(std::move(f));
    };
  }

 private:
  std::shared_ptr<MessageBatch> child_;
  std::mutex mu_;
  std::vector<opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>>
      message_spans_;  // ABSL_GUARDED_BY(mu_)
};

std::shared_ptr<MessageBatch> MakeTracingMessageBatch(
    std::shared_ptr<MessageBatch> message_batch) {
  return std::make_shared<TracingMessageBatch>(std::move(message_batch));
}

#else  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::shared_ptr<MessageBatch> MakeTracingMessageBatch(
    std::shared_ptr<MessageBatch> message_batch) {
  return message_batch;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
