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

#include "google/cloud/pubsub/internal/batch_callback.h"
#include "google/cloud/pubsub/internal/message_propagator.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/opentelemetry.h"
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <google/pubsub/v1/pubsub.pb.h>
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/trace/semantic_conventions.h>
#include <opentelemetry/trace/span_startoptions.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace {

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> StartSubscribeSpan(
    google::pubsub::v1::ReceivedMessage const& message,
    pubsub::Subscription const& subscription,
    std::shared_ptr<
        opentelemetry::context::propagation::TextMapPropagator> const&
        propagator,
    bool exactly_once_delivery_enabled) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  opentelemetry::trace::StartSpanOptions options;
  options.kind = opentelemetry::trace::SpanKind::kConsumer;
  auto m = pubsub_internal::FromProto(message.message());
  auto context = ExtractTraceContext(m, *propagator);
  auto producer_span_context =
      opentelemetry::trace::GetSpan(context)->GetContext();
  if (producer_span_context.IsSampled() && producer_span_context.IsValid()) {
    options.parent = producer_span_context;
  }

  auto span = internal::MakeSpan(
      subscription.subscription_id() + " subscribe",
      {{sc::kMessagingSystem, "gcp_pubsub"},
       {/*sc::kMessagingOperationType=*/"messaging.operation.type",
        "subscribe"},
       {"gcp.project_id", subscription.project_id()},
       {sc::kMessagingDestinationName, subscription.subscription_id()},
       {sc::kMessagingMessageId, m.message_id()},
       {/*sc::kMessagingMessageEnvelopeSize=*/"messaging.message.envelope."
                                              "size",
        static_cast<std::int64_t>(MessageSize(m))},
       {"messaging.gcp_pubsub.message.ack_id", message.ack_id()},
       {"messaging.gcp_pubsub.subscription.exactly_once_delivery",
        exactly_once_delivery_enabled}},
      options);

  if (!message.message().ordering_key().empty()) {
    span->SetAttribute("messaging.gcp_pubsub.message.ordering_key",
                       message.message().ordering_key());
  }
  return span;
}

/**
 * Tracing batch callback implementation.
 */
class TracingBatchCallback : public BatchCallback {
 public:
  TracingBatchCallback(std::shared_ptr<BatchCallback> child,
                       pubsub::Subscription subscription)
      : child_(std::move(child)),
        subscription_(std::move(subscription)),
        propagator_(std::make_shared<
                    opentelemetry::trace::propagation::HttpTraceContext>()) {}

  ~TracingBatchCallback() override {
    std::lock_guard<std::mutex> lk(mu_);
    // End all outstanding spans.
    for (auto const& kv : spans_by_ack_id_) {
      if (kv.second.subscribe_span) kv.second.subscribe_span->End();
      if (kv.second.concurrency_control_span) {
        kv.second.concurrency_control_span->End();
      }
      if (kv.second.scheduler_span) kv.second.scheduler_span->End();
    }
    spans_by_ack_id_.clear();
  }

  void callback(BatchCallback::StreamingPullResponse response) override {
    if (response.response) {
      for (auto const& message : response.response->received_messages()) {
        auto exactly_once_delivery_enabled = false;
        if (response.response->has_subscription_properties()) {
          exactly_once_delivery_enabled =
              response.response->subscription_properties()
                  .exactly_once_delivery_enabled();
        }
        auto subscribe_span = StartSubscribeSpan(
            message, subscription_, propagator_, exactly_once_delivery_enabled);
        std::lock_guard<std::mutex> lk(mu_);
        spans_by_ack_id_[message.ack_id()].subscribe_span = subscribe_span;
      }
    }
    child_->callback(std::move(response));
  };

  void message_callback(ReceivedMessage m) override {
    child_->message_callback(std::move(m));
  }

  void user_callback(MessageCallback::MessageAndHandler m) override {
    std::unique_lock<std::mutex> lk(mu_);
    auto it = spans_by_ack_id_.find(m.ack_id);
    if (it != spans_by_ack_id_.end()) {
      // Takes the subscribe span from the TracingBatchCallback and passes it to
      // the MessageCallback.
      m.subscribe_span.span = it->second.subscribe_span;
    }
    // Don't hold the lock while the callback executes.
    lk.unlock();
    child_->user_callback(std::move(m));
  }

  void StartConcurrencyControl(std::string const& ack_id) override {
    namespace sc = opentelemetry::trace::SemanticConventions;
    std::lock_guard<std::mutex> lk(mu_);
    auto it = spans_by_ack_id_.find(ack_id);
    if (it == spans_by_ack_id_.end()) return;
    auto subscribe_span = it->second.subscribe_span;
    if (!subscribe_span) return;
    opentelemetry::trace::StartSpanOptions options;
    options.parent = subscribe_span->GetContext();
    it->second.concurrency_control_span =
        internal::MakeSpan("subscriber concurrency control",
                           {{sc::kMessagingSystem, "gcp_pubsub"}}, options);
  }

  void EndConcurrencyControl(std::string const& ack_id) override {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = spans_by_ack_id_.find(ack_id);
    if (it != spans_by_ack_id_.end()) {
      auto concurrency_control_span =
          std::move(it->second.concurrency_control_span);
      if (concurrency_control_span) {
        concurrency_control_span->End();
      }
    }
  }

  void StartScheduler(std::string const& ack_id) override {
    namespace sc = opentelemetry::trace::SemanticConventions;
    std::lock_guard<std::mutex> lk(mu_);
    auto spans = spans_by_ack_id_.find(ack_id);
    if (spans == spans_by_ack_id_.end()) return;
    auto subscribe_span = spans->second.subscribe_span;
    if (!subscribe_span) return;
    opentelemetry::trace::StartSpanOptions options;
    options.parent = subscribe_span->GetContext();
    spans->second.scheduler_span =
        internal::MakeSpan("subscriber scheduler",
                           {{sc::kMessagingSystem, "gcp_pubsub"}}, options);
  }

  void EndScheduler(std::string const& ack_id) override {
    std::lock_guard<std::mutex> lk(mu_);
    auto spans = spans_by_ack_id_.find(ack_id);
    if (spans == spans_by_ack_id_.end()) return;
    auto scheduler_span = std::move(spans->second.scheduler_span);
    if (scheduler_span) scheduler_span->End();
  }

  Span StartModackSpan(
      google::pubsub::v1::ModifyAckDeadlineRequest const& request) override {
    namespace sc = opentelemetry::trace::SemanticConventions;
    using Attributes =
        std::vector<std::pair<opentelemetry::nostd::string_view,
                              opentelemetry::common::AttributeValue>>;
    using Links =
        std::vector<std::pair<opentelemetry::trace::SpanContext, Attributes>>;

    Links links;
    std::unique_lock<std::mutex> lk(mu_);
    for (auto const& ack_id : request.ack_ids()) {
      auto spans = spans_by_ack_id_.find(ack_id);
      if (spans != spans_by_ack_id_.end()) {
        links.emplace_back(std::make_pair(
            spans->second.subscribe_span->GetContext(), Attributes{}));
      }
    }
    lk.unlock();

    opentelemetry::trace::StartSpanOptions options;
    options.kind = opentelemetry::trace::SpanKind::kClient;
    auto span = internal::MakeSpan(
        subscription_.subscription_id() + " modack",
        {{sc::kMessagingSystem, "gcp_pubsub"},
         {/*sc::kMessagingOperationType=*/"messaging.operation.type", "extend"},
         {sc::kMessagingBatchMessageCount,
          static_cast<int64_t>(request.ack_ids().size())},
         {"messaging.gcp_pubsub.message.ack_deadline_seconds",
          static_cast<int64_t>(request.ack_deadline_seconds())},
         {sc::kMessagingDestinationName, subscription_.subscription_id()},
         {"gcp.project_id", subscription_.project_id()}},
        std::move(links), options);

    return Span{span};
  }

  void EndModackSpan(Span span) override { span.span->End(); }

  void AckStart(std::string const& ack_id) override {
    AddEvent(ack_id, "gl-cpp.ack_start");
  }
  void AckEnd(std::string const& ack_id) override {
    AddEvent(ack_id, "gl-cpp.ack_end", true);
  }

  void NackStart(std::string const& ack_id) override {
    AddEvent(ack_id, "gl-cpp.nack_start");
  }
  void NackEnd(std::string const& ack_id) override {
    AddEvent(ack_id, "gl-cpp.nack_end", true);
  }

  void ModackStart(std::string const& ack_id) override {
    AddEvent(ack_id, "gl-cpp.modack_start");
  }
  void ModackEnd(std::string const& ack_id) override {
    AddEvent(ack_id, "gl-cpp.modack_end");
  }

  void ExpireMessage(std::string const& ack_id) override {
    AddEvent(ack_id, "gl-cpp.expired");
  }

 private:
  struct MessageSpans {
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> subscribe_span;
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>
        concurrency_control_span;
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> scheduler_span;
  };

  void AddEvent(std::string const& ack_id, std::string const& event,
                bool end_event = false) {
    std::lock_guard<std::mutex> lk(mu_);
    // Use the ack_id to find the subscribe span and add an event to it.
    auto it = spans_by_ack_id_.find(ack_id);
    if (it == spans_by_ack_id_.end()) return;
    auto subscribe_span = it->second.subscribe_span;
    subscribe_span->AddEvent(event);
    if (event == "gl-cpp.ack_end") {
      subscribe_span->SetAttribute("messaging.gcp_pubsub.result", "ack");
    } else if (event == "gl-cpp.nack_end") {
      subscribe_span->SetAttribute("messaging.gcp_pubsub.result", "nack");
    } else if (event == "gl-cpp.expired") {
      subscribe_span->SetAttribute("messaging.gcp_pubsub.result", "expired");
    }
    if (end_event) {
      subscribe_span->End();
      spans_by_ack_id_.erase(it);
    }
  }

  std::shared_ptr<BatchCallback> child_;
  pubsub::Subscription subscription_;
  std::shared_ptr<opentelemetry::context::propagation::TextMapPropagator>
      propagator_;
  std::mutex mu_;
  std::unordered_map<std::string, MessageSpans>
      spans_by_ack_id_;  // ABSL_GUARDED_BY(mu_)
};

}  // namespace

std::shared_ptr<BatchCallback> MakeTracingBatchCallback(
    std::shared_ptr<BatchCallback> batch_callback,
    pubsub::Subscription const& subscription) {
  return std::make_shared<TracingBatchCallback>(std::move(batch_callback),
                                                subscription);
}

#else  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::shared_ptr<BatchCallback> MakeTracingBatchCallback(
    std::shared_ptr<BatchCallback> batch_callback,
    pubsub::Subscription const&) {
  return batch_callback;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
