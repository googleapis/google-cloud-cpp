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

void TracingMessageBatch::Flush() {
  using opentelemetry::trace::SpanContext;
  using AttributesList =
      std::vector<std::pair<opentelemetry::nostd::string_view,
                            opentelemetry::common::AttributeValue>>;
  auto constexpr kMaxOtelLinks = 128;
  std::vector<std::pair<SpanContext, AttributesList>> links;

  // If the batch size is less than the max size, add the links to a single
  // span.
  if (message_spans_.size() < kMaxOtelLinks) {
    std::transform(
        message_spans_.begin(), message_spans_.end(), std::back_inserter(links),
        [i = std::int64_t(0)](auto const& span) mutable {
          return std::make_pair(
              span->GetContext(),
              AttributesList{{"messaging.pubsub.message.link", i++}});
        });
  }
  auto batch_sink_span_parent = internal::MakeSpan(
      "BatchSink::AsyncPublish",
      /*attributes=*/
      {{"messaging.pubsub.num_messages_in_batch", message_count}},
      /*links*/ links);

  // TODO(#12528): Handle batches larger than 128.

  // Clear message spans.
  message_spans_.clear();

  batch_sink_spans_.push_back(batch_sink_span_parent);

  // Set the batch sink parent span.
  auto async_scope = internal::GetTracer(internal::CurrentOptions())
                         ->WithActiveSpan(batch_sink_span_parent);
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
