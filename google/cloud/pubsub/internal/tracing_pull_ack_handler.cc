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

class TracingPullAckHandler : public pubsub::PullAckHandler::Impl {
 public:
  explicit TracingPullAckHandler(
      std::unique_ptr<pubsub::PullAckHandler::Impl> child,
      pubsub::Subscription const& subscription, std::string ack_id)
      : child_(std::move(child)),
        subscription_(std::move(subscription)),
        ack_id_(std::move(ack_id)) {}
  ~TracingPullAckHandler() override = default;

  Attributes MakeSharedAttributes() {
    namespace sc = opentelemetry::trace::SemanticConventions;
    return Attributes{{sc::kMessagingSystem, "gcp_pubsub"},
                      {sc::kMessagingOperation, "settle"},
                      {"messaging.gcp_pubsub.message.ack_id", ack_id_},
                      {"messaging.gcp_pubsub.message.delivery_attempt",
                       child_->delivery_attempt()}};
  }

  future<Status> ack() override {
    namespace sc = opentelemetry::trace::SemanticConventions;
    opentelemetry::trace::StartSpanOptions options;
    options.kind = opentelemetry::trace::SpanKind::kClient;
    Attributes attributes = MakeSharedAttributes();
    attributes.emplace_back(
        std::make_pair(sc::kCodeFunction, "pubsub::PullAckHandler::ack"));
    auto span = internal::MakeSpan(subscription_.subscription_id() + " settle",
                                   attributes, options);
    auto scope = internal::OTelScope(span);

    return child_->ack().then(
        [oc = opentelemetry::context::RuntimeContext::GetCurrent(),
         subscription = std::move(subscription_), span =
             std::move(span)](auto f) {
  auto result = f.get();
          internal::DetachOTelContext(oc);
          return internal::EndSpan(*span, std::move(result));
        });
  }

  future<Status> nack() override {
    namespace sc = opentelemetry::trace::SemanticConventions;
    opentelemetry::trace::StartSpanOptions options;
    options.kind = opentelemetry::trace::SpanKind::kClient;
    Attributes attributes = MakeSharedAttributes();
    attributes.emplace_back(
        std::make_pair(sc::kCodeFunction, "pubsub::PullAckHandler::nack"));
    auto span = internal::MakeSpan(subscription_.subscription_id() + " settle",
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

 private:
  std::unique_ptr<pubsub::PullAckHandler::Impl> child_;
  pubsub::Subscription subscription_;
  std::string ack_id_;
};

std::unique_ptr<pubsub::PullAckHandler::Impl> MakeTracingPullAckHandler(
    std::unique_ptr<pubsub::PullAckHandler::Impl> handler,
    pubsub::Subscription subscription, std::string ack_id) {
  return std::make_unique<TracingPullAckHandler>(
      std::move(handler), std::move(subscription), std::move(ack_id));
}

#else  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::unique_ptr<pubsub::PullAckHandler::Impl> MakeTracingPullAckHandler(
    std::unique_ptr<pubsub::PullAckHandler::Impl> handler,
    pubsub::Subscription subscription, std::string ack_id) {
  return handler;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
