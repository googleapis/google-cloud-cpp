// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/internal/tracing_pull_ack_handler.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/status.h"
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "opentelemetry/context/runtime_context.h"
#include "opentelemetry/trace/context.h"
#include "opentelemetry/trace/semantic_conventions.h"
#include "opentelemetry/trace/span.h"
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <cstdint>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using Attributes =
    std::vector<std::pair<opentelemetry::nostd::string_view,
                          opentelemetry::common::AttributeValue>>;

namespace {

auto CreateLinks() {
  using Links =
      std::vector<std::pair<opentelemetry::trace::SpanContext, Attributes>>;
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  auto consumer_span_context =
      opentelemetry::trace::GetSpan(
          opentelemetry::context::RuntimeContext::GetCurrent())
          ->GetContext();
  if (consumer_span_context.IsSampled() && consumer_span_context.IsValid()) {
    return Links{{consumer_span_context, Attributes{}}};
  }
#endif
  return Links{};
}

void MaybeAddLinkAttributes(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> const& span) {
#if OPENTELEMETRY_ABI_VERSION_NO < 2
  auto consumer_span_context =
      opentelemetry::trace::GetSpan(
          opentelemetry::context::RuntimeContext::GetCurrent())
          ->GetContext();
  if (consumer_span_context.IsSampled() && consumer_span_context.IsValid()) {
    span->SetAttribute("gcp_pubsub.receive.trace_id",
                       internal::ToString(consumer_span_context.trace_id()));
    span->SetAttribute("gcp_pubsub.receive.span_id",
                       internal::ToString(consumer_span_context.span_id()));
  }
#else
  (void)span;
#endif
}

}  // namespace

class TracingPullAckHandler : public pubsub::PullAckHandler::Impl {
 public:
  explicit TracingPullAckHandler(
      std::unique_ptr<pubsub::PullAckHandler::Impl> child)
      : child_(std::move(child)) {}
  ~TracingPullAckHandler() override = default;

  Attributes MakeSharedAttributes(std::string const& ack_id,
                                  std::string const& subscription) {
    namespace sc = opentelemetry::trace::SemanticConventions;
    return Attributes{
        {sc::kMessagingSystem, "gcp_pubsub"},
        {sc::kMessagingOperation, "settle"},
        {"messaging.gcp_pubsub.message.ack_id", ack_id},
        {"messaging.gcp_pubsub.message.delivery_attempt",
         child_->delivery_attempt()},
        {"messaging.gcp_pubsub.subscription.template", subscription}};
  }

  future<Status> ack() override {
    namespace sc = opentelemetry::trace::SemanticConventions;
    opentelemetry::trace::StartSpanOptions options;
    options.kind = opentelemetry::trace::SpanKind::kClient;
    auto const ack_id = child_->ack_id();
    auto const subscription = child_->subscription();
    auto const subscription_name = subscription.FullName();
    Attributes attributes = MakeSharedAttributes(ack_id, subscription_name);
    attributes.emplace_back(
        std::make_pair(sc::kCodeFunction, "pubsub::PullAckHandler::ack"));
    auto span = internal::MakeSpan(subscription.subscription_id() + " settle",
                                   attributes, CreateLinks(), options);
    MaybeAddLinkAttributes(span);
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
    namespace sc = opentelemetry::trace::SemanticConventions;
    opentelemetry::trace::StartSpanOptions options;
    options.kind = opentelemetry::trace::SpanKind::kClient;
    auto const ack_id = child_->ack_id();
    auto const subscription = child_->subscription();
    auto const subscription_name = subscription.FullName();
    Attributes attributes = MakeSharedAttributes(ack_id, subscription_name);
    attributes.emplace_back(
        std::make_pair(sc::kCodeFunction, "pubsub::PullAckHandler::nack"));
    auto span = internal::MakeSpan(subscription.subscription_id() + " settle",
                                   attributes, CreateLinks(), options);
    MaybeAddLinkAttributes(span);
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

  std::string ack_id() const override { return child_->ack_id(); }

  pubsub::Subscription subscription() const override {
    return child_->subscription();
  }

 private:
  std::unique_ptr<pubsub::PullAckHandler::Impl> child_;
};

std::unique_ptr<pubsub::PullAckHandler::Impl> MakeTracingPullAckHandler(
    std::unique_ptr<pubsub::PullAckHandler::Impl> handler) {
  return std::make_unique<TracingPullAckHandler>(std::move(handler));
}

#else  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::unique_ptr<pubsub::PullAckHandler::Impl> MakeTracingPullAckHandler(
    std::unique_ptr<pubsub::PullAckHandler::Impl> handler) {
  return handler;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
