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

#include "google/cloud/pubsub/exactly_once_ack_handler.h"
#include "google/cloud/pubsub/internal/span.h"
#include "google/cloud/pubsub/internal/tracing_helpers.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/status.h"
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/trace/context.h>
#include <opentelemetry/trace/semantic_conventions.h>
#include <opentelemetry/trace/span.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <cstdint>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace {

class TracingExactlyOnceAckHandler
    : public pubsub::ExactlyOnceAckHandler::Impl {
 public:
  explicit TracingExactlyOnceAckHandler(
      std::unique_ptr<pubsub::ExactlyOnceAckHandler::Impl> child,
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>
          subscribe_span)
      : child_(std::move(child)), subscribe_span_(std::move(subscribe_span)) {}
  using Attributes =
      std::vector<std::pair<opentelemetry::nostd::string_view,
                            opentelemetry::common::AttributeValue>>;
  using Links =
      std::vector<std::pair<opentelemetry::trace::SpanContext, Attributes>>;

  ~TracingExactlyOnceAckHandler() override = default;
  future<Status> ack() override {
    // Check that the subscribe span exists. A span could not exist if it
    // expired before the ack or has already been acked/nacked.
    Links links;
    if (subscribe_span_) {
      subscribe_span_->AddEvent("gl-cpp.message_ack");
      links.emplace_back(
          std::make_pair(subscribe_span_->GetContext(), Attributes{}));
    }
    namespace sc = opentelemetry::trace::SemanticConventions;
    opentelemetry::trace::StartSpanOptions options = RootStartSpanOptions();
    options.kind = opentelemetry::trace::SpanKind::kInternal;
    auto sub = subscription();
    auto span = internal::MakeSpan(
        sub.subscription_id() + " ack",
        {{sc::kCodeFunction, "pubsub::AckHandler::ack"},
         {sc::kMessagingSystem, "gcp_pubsub"},
         {"messaging.gcp_pubsub.message.ack_id", ack_id()},
         {"messaging.gcp_pubsub.subscription.template", sub.FullName()},
         {"gcp.project_id", sub.project_id()},
         {sc::kMessagingDestinationName, sub.subscription_id()},
         {"messaging.gcp_pubsub.message.delivery_attempt",
          static_cast<int32_t>(delivery_attempt())},
         {sc::kMessagingOperation, "settle"}},
        std::move(links), options);
    auto scope = internal::OTelScope(span);
    return internal::EndSpan(std::move(span), child_->ack());
  }

  future<Status> nack() override {
    // Check that the subscribe span exists. A span could not exist if it
    // expired before the ack or has already been acked/nacked.
    Links links;
    if (subscribe_span_) {
      subscribe_span_->AddEvent("gl-cpp.message_nack");
      links.emplace_back(
          std::make_pair(subscribe_span_->GetContext(), Attributes{}));
    }
    namespace sc = opentelemetry::trace::SemanticConventions;
    opentelemetry::trace::StartSpanOptions options = RootStartSpanOptions();
    options.kind = opentelemetry::trace::SpanKind::kInternal;
    auto sub = subscription();
    auto span = internal::MakeSpan(
        sub.subscription_id() + " nack",
        {{sc::kCodeFunction, "pubsub::AckHandler::nack"},
         {sc::kMessagingSystem, "gcp_pubsub"},
         {"messaging.gcp_pubsub.message.ack_id", ack_id()},
         {"messaging.gcp_pubsub.subscription.template", sub.FullName()},
         {"gcp.project_id", sub.project_id()},
         {sc::kMessagingDestinationName, sub.subscription_id()},
         {"messaging.gcp_pubsub.message.delivery_attempt",
          static_cast<int32_t>(delivery_attempt())},
         {sc::kMessagingOperation, "settle"}},
        std::move(links), options);

    auto scope = internal::OTelScope(span);
    return internal::EndSpan(std::move(span), child_->nack());
  }

  std::int32_t delivery_attempt() const override {
    return child_->delivery_attempt();
  }

  std::string ack_id() override { return child_->ack_id(); }

  pubsub::Subscription subscription() const override {
    return child_->subscription();
  }

 private:
  std::unique_ptr<pubsub::ExactlyOnceAckHandler::Impl> child_;
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> subscribe_span_;
};

}  // namespace

std::unique_ptr<pubsub::ExactlyOnceAckHandler::Impl>
MakeTracingExactlyOnceAckHandler(
    std::unique_ptr<pubsub::ExactlyOnceAckHandler::Impl> handler,
    Span const& span) {
  return std::make_unique<TracingExactlyOnceAckHandler>(std::move(handler),
                                                        span.span);
}

#else  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::unique_ptr<pubsub::ExactlyOnceAckHandler::Impl>
MakeTracingExactlyOnceAckHandler(
    std::unique_ptr<pubsub::ExactlyOnceAckHandler::Impl> handler, Span const&) {
  return handler;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
