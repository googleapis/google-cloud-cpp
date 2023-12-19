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

#include "google/cloud/pubsub/internal/tracing_batch_sink.h"
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/pubsub/internal/publisher_stub.h"
#include "google/cloud/pubsub/options.h"
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

/// Creates a link for each sampled span in the range @p begin to @p end.
auto MakeLinks(Spans::const_iterator begin, Spans::const_iterator end) {
  Links links;
  Spans sampled_spans;
  std::copy_if(begin, end, std::back_inserter(sampled_spans),
               [](auto const& span) { return span->GetContext().IsSampled(); });
  std::transform(sampled_spans.begin(), sampled_spans.end(),
                 std::back_inserter(links), [](auto const& span) mutable {
                   return std::make_pair(span->GetContext(), Attributes{});
                 });
  return links;
}

auto MakeParent(Links const& links, Spans const& message_spans,
                pubsub::Topic const& topic) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  opentelemetry::trace::StartSpanOptions options;
  opentelemetry::context::Context root_context;
  // TODO(#13287): Use the constant instead of the string.
  // Setting a span as a root span was added in OTel v1.13+. It is a no-op for
  // earlier versions.
  options.parent = root_context.SetValue(
      /*opentelemetry::trace::kIsRootSpanKey=*/"is_root_span", true);
  options.kind = opentelemetry::trace::SpanKind::kClient;
  auto batch_sink_parent = internal::MakeSpan(
      topic.topic_id() + " publish",
      /*attributes=*/
      {{sc::kMessagingBatchMessageCount,
        static_cast<std::int64_t>(message_spans.size())},
       {sc::kCodeFunction, "BatchSink::AsyncPublish"},
       {/*sc::kMessagingOperation=*/
        "messaging.operation", "publish"},
       {sc::kThreadId, internal::CurrentThreadId()},
       {sc::kMessagingSystem, "gcp_pubsub"},
       {sc::kMessagingDestinationTemplate, topic.FullName()}},
      /*links*/ std::move(links), options);

  auto context = batch_sink_parent->GetContext();
  auto trace_id = internal::ToString(context.trace_id());
  auto span_id = internal::ToString(context.span_id());
  for (auto const& message_span : message_spans) {
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
    message_span->AddEvent("gl-cpp.publish_start");
    message_span->AddLink(context, {{}});
#else
    message_span->AddEvent("gl-cpp.publish_start",
                           Attributes{{"gcp_pubsub.publish.trace_id", trace_id},
                                      {"gcp_pubsub.publish.span_id", span_id}});
#endif
  }
  return batch_sink_parent;
}

auto MakeChild(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> const& parent,
    int count, Links const& links) {
  opentelemetry::trace::StartSpanOptions options;
  options.parent = parent->GetContext();
  options.kind = opentelemetry::trace::SpanKind::kClient;
  return internal::MakeSpan("publish #" + std::to_string(count),
                            /*attributes=*/{{}},
                            /*links=*/links, options);
}

Spans MakeBatchSinkSpans(Spans const& message_spans, pubsub::Topic const& topic,
                         Options const& options) {
  auto const max_otel_links = options.get<pubsub::MaxOtelLinkCountOption>();
  Spans batch_sink_spans;
  // If the batch size is less than the max size, add the links to a single
  // span. If the batch size is greater than the max size, create a parent
  // span with no links and each child spans will contain links.
  if (message_spans.size() <= max_otel_links) {
    batch_sink_spans.push_back(
        MakeParent(MakeLinks(message_spans.begin(), message_spans.end()),
                   message_spans, topic));
    return batch_sink_spans;
  }
  batch_sink_spans.push_back(MakeParent({{}}, message_spans, topic));
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

/**
 * Records spans related to a batch of messages across calls and
 * callbacks in the `BatchingPublisherConnection`.
 */
class TracingBatchSink : public BatchSink {
 public:
  explicit TracingBatchSink(pubsub::Topic topic,
                            std::shared_ptr<BatchSink> child, Options opts)
      : topic_(std::move(topic)),
        child_(std::move(child)),
        options_(std::move(opts)) {}

  ~TracingBatchSink() override = default;

  void AddMessage(pubsub::Message const& m) override {
    auto active_span = opentelemetry::trace::GetSpan(
        opentelemetry::context::RuntimeContext::GetCurrent());
    active_span->AddEvent("gl-cpp.added_to_batch");
    {
      std::lock_guard<std::mutex> lk(mu_);
      message_spans_.push_back(std::move(active_span));
    }
    child_->AddMessage(std::move(m));
  }

  future<StatusOr<google::pubsub::v1::PublishResponse>> AsyncPublish(
      google::pubsub::v1::PublishRequest request) override {
    decltype(message_spans_) message_spans;
    {
      std::lock_guard<std::mutex> lk(mu_);
      message_spans.swap(message_spans_);
    }

    auto batch_sink_spans = MakeBatchSinkSpans(message_spans, topic_, options_);

    // The first span in `batch_sink_spans` is the parent to the other spans in
    // the vector.
    internal::OTelScope scope(batch_sink_spans.front());
    return child_->AsyncPublish(std::move(request))
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
               spans = std::move(batch_sink_spans),
               message_spans = std::move(message_spans)](auto f) mutable {
          for (auto& span : message_spans) {
            span->AddEvent("gl-cpp.publish_end");
          }
          for (auto& span : spans) {
            internal::EndSpan(*span);
          }
          internal::DetachOTelContext(oc);
          return f.get();
        });
  }

  void ResumePublish(std::string const& ordering_key) override {
    child_->ResumePublish(ordering_key);
  };

 private:
  pubsub::Topic topic_;
  std::shared_ptr<BatchSink> child_;
  std::mutex mu_;
  std::vector<opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>>
      message_spans_;  // ABSL_GUARDED_BY(mu_)
  Options options_;
};

}  // namespace

std::shared_ptr<BatchSink> MakeTracingBatchSink(
    pubsub::Topic topic, std::shared_ptr<BatchSink> batch_sink, Options opts) {
  return std::make_shared<TracingBatchSink>(
      std::move(topic), std::move(batch_sink), std::move(opts));
}

#else  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::shared_ptr<BatchSink> MakeTracingBatchSink(
    pubsub::Topic, std::shared_ptr<BatchSink> batch_sink, Options) {
  return batch_sink;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
