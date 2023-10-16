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
#include "google/cloud/pubsub/internal/message_batch.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/opentelemetry.h"
#include "opentelemetry/context/runtime_context.h"
#include "opentelemetry/trace/context.h"
#include "opentelemetry/trace/span.h"
#include <algorithm>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void TracingMessageBatch::SaveMessage(pubsub::Message m) {
  auto active_span = opentelemetry::trace::GetSpan(
      opentelemetry::context::RuntimeContext::GetCurrent());
  active_span->AddEvent("gl-cpp.added_to_batch");
  {
    std::lock_guard<std::mutex> lk(mu_);
    message_spans_.push_back(std::move(active_span));
  }
  child_->SaveMessage(std::move(m));
}

std::string ToString(opentelemetry::trace::TraceId trace_id) {
  char trace_id_array[32] = {0};
  trace_id.ToLowerBase16(trace_id_array);
  return std::string(trace_id_array, 32);
}

std::string ToString(opentelemetry::trace::SpanId span_id) {
  char span_id_array[16] = {0};
  span_id.ToLowerBase16(span_id_array);
  return std::string(span_id_array, 16);
}

void TracingMessageBatch::AddMessageSpanMetadata() {
  using opentelemetry::trace::SpanId;
  using opentelemetry::trace::TraceId;
  auto context = batch_sink_parent_span_->GetContext();
  auto trace_id = ToString(context.trace_id());
  auto span_id = ToString(context.span_id());
  for (auto& message_span : message_spans_) {
    message_span->AddEvent("gl-cpp.batch_flushed");
    message_span->SetAttribute("pubsub.batch_sink.trace_id", trace_id);
    message_span->SetAttribute("pubsub.batch_sink.span_id", span_id);
  }
}

void TracingMessageBatch::Flush() {
  using opentelemetry::trace::SpanContext;
  using AttributesList =
      std::vector<std::pair<opentelemetry::nostd::string_view,
                            opentelemetry::common::AttributeValue>>;
  auto constexpr kMaxOtelLinks = 128;
  std::vector<std::pair<SpanContext, AttributesList>> links;
  auto batch_size = message_spans_.size();
  links.reserve(batch_size);

  // If the batch size is less than the max size, add the links to a single
  // span.
  if (batch_size < kMaxOtelLinks) {
    std::transform(
        message_spans_.begin(), message_spans_.end(), std::back_inserter(links),
        [i = static_cast<std::int64_t>(0)](auto const& span) mutable {
          return std::make_pair(
              span->GetContext(),
              AttributesList{{"messaging.pubsub.message.link", i++}});
        });
  }
  batch_sink_parent_span_ =
      internal::MakeSpan("BatchSink::AsyncPublish",
                         /*attributes=*/
                         {{"messaging.pubsub.num_messages_in_batch",
                           static_cast<std::int64_t>(batch_size)}},
                         /*links*/ links);

  // TODO(#12528): Handle batches larger than 128.

  // This must be called before we clear the message spans.
  AddMessageSpanMetadata();
  // Clear message spans.
  message_spans_.clear();

  batch_sink_spans_.push_back(batch_sink_parent_span_);

  // Set the batch sink parent span.
  auto async_scope = internal::GetTracer(internal::CurrentOptions())
                         ->WithActiveSpan(batch_sink_parent_span_);

  child_->Flush();
}

void TracingMessageBatch::FlushCallback() {
  decltype(batch_sink_spans_) spans;
  {
    std::lock_guard<std::mutex> lk(mu_);
    spans.swap(batch_sink_spans_);
  }
  for (auto& span : spans) internal::EndSpan(*span);
  child_->FlushCallback();
}

void TracingMessageBatch::SetBatchSinkParentSpan(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span) {
  batch_sink_parent_span_ = span;
}

std::vector<opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>>
TracingMessageBatch::GetMessageSpans() const {
  return message_spans_;
}

std::vector<opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>>
TracingMessageBatch::GetBatchSinkSpans() const {
  return batch_sink_spans_;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
