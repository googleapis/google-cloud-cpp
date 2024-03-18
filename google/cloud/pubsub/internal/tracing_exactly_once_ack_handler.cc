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
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/status.h"
#include <memory>
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/pubsub/internal/tracing_helpers.h"
#include "opentelemetry/context/runtime_context.h"
#include "opentelemetry/trace/context.h"
#include "opentelemetry/trace/semantic_conventions.h"
#include "opentelemetry/trace/span.h"
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

class TracingExactlyOnceAckHandler
    : public pubsub::ExactlyOnceAckHandler::Impl {
 public:
  explicit TracingExactlyOnceAckHandler(
      std::unique_ptr<pubsub::ExactlyOnceAckHandler::Impl> child,
      absl::optional<
          opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>>
          subscribe_span)
      : child_(std::move(child)), subscribe_span_(std::move(subscribe_span)) {}

  ~TracingExactlyOnceAckHandler() override = default;
  future<Status> ack() override {
    if (subscribe_span_.has_value()) {
      subscribe_span_.value()->AddEvent("gl-cpp.message_ack");
    }
    namespace sc = opentelemetry::trace::SemanticConventions;
    opentelemetry::trace::StartSpanOptions options = RootStartSpanOptions();

    options.kind = opentelemetry::trace::SpanKind::kInternal;
    auto const ack_id = child_->ack_id();
    auto const subscription = child_->subscription();
    TracingAttributes attributes = MakeSharedAttributes(ack_id, subscription);
    attributes.emplace_back(
        std::make_pair(sc::kCodeFunction, "pubsub::AckHandler::ack"));
    auto span = internal::MakeSpan(subscription.subscription_id() + " ack",
                                   attributes, options);

    auto scope = internal::OTelScope(span);
    return child_->ack().then(
        [oc = opentelemetry::context::RuntimeContext::GetCurrent(),
         span = std::move(span)](auto f) {
          auto result = f.get();
          internal::DetachOTelContext(oc);
          return internal::EndSpan(*span, std::move(result));
        });
  }

  future<Status> nack() override {
    if (subscribe_span_.has_value()) {
      subscribe_span_.value()->AddEvent("gl-cpp.message_nack");
    }
    namespace sc = opentelemetry::trace::SemanticConventions;
    opentelemetry::trace::StartSpanOptions options = RootStartSpanOptions();

    options.kind = opentelemetry::trace::SpanKind::kInternal;
    auto const ack_id = child_->ack_id();
    auto const subscription = child_->subscription();
    TracingAttributes attributes = MakeSharedAttributes(ack_id, subscription);
    attributes.emplace_back(
        std::make_pair(sc::kCodeFunction, "pubsub::AckHandler::nack"));
    auto span = internal::MakeSpan(subscription.subscription_id() + " nack",
                                   attributes, options);

    auto scope = internal::OTelScope(span);
    return child_->nack().then(
        [oc = opentelemetry::context::RuntimeContext::GetCurrent(),
         span = std::move(span)](auto f) {
          auto result = f.get();
          internal::DetachOTelContext(oc);
          return internal::EndSpan(*span, std::move(result));
        });
  }

  std::int32_t delivery_attempt() const override {
    return child_->delivery_attempt();
  }

  std::string ack_id() override { return child_->ack_id(); }

  pubsub::Subscription subscription() const override {
    return child_->subscription();
  }

 private:
  TracingAttributes MakeSharedAttributes(
      std::string const& ack_id, pubsub::Subscription const& subscription) {
    namespace sc = opentelemetry::trace::SemanticConventions;
    return TracingAttributes{
        {sc::kMessagingSystem, "gcp_pubsub"},
        {"messaging.gcp_pubsub.message.ack_id", ack_id},
        {"messaging.gcp_pubsub.subscription.template", subscription.FullName()},
        {"gcp.project_id", subscription.project_id()},
        {sc::kMessagingDestinationName, subscription.subscription_id()},
        {"messaging.gcp_pubsub.message.delivery_attempt",
         static_cast<int32_t>(child_->delivery_attempt())},
        {sc::kMessagingOperation, "settle"}};
  }

  std::unique_ptr<pubsub::ExactlyOnceAckHandler::Impl> child_;
  absl::optional<opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>>
      subscribe_span_;
};

std::unique_ptr<pubsub::ExactlyOnceAckHandler::Impl>
MakeTracingExactlyOnceAckHandler(
    std::unique_ptr<pubsub::ExactlyOnceAckHandler::Impl> handler, Span span) {
  return std::make_unique<TracingExactlyOnceAckHandler>(std::move(handler),
                                                        span.GetSpan());
}

#else  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::unique_ptr<pubsub::ExactlyOnceAckHandler::Impl>
MakeTracingExactlyOnceAckHandler(
    std::unique_ptr<pubsub::ExactlyOnceAckHandler::Impl> handler, Span) {
  return handler;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
