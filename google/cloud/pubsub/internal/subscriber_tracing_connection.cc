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

#include "google/cloud/pubsub/internal/subscriber_tracing_connection.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/opentelemetry.h"
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "opentelemetry/trace/semantic_conventions.h"
#include "opentelemetry/trace/span_startoptions.h"
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace {

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> StartPullSpan() {
  auto const& current = internal::CurrentOptions();
  auto const& subscription = current.get<pubsub::SubscriptionOption>();
  namespace sc = opentelemetry::trace::SemanticConventions;
  opentelemetry::trace::StartSpanOptions options;
  options.kind = opentelemetry::trace::SpanKind::kConsumer;
  auto span = internal::MakeSpan(
      subscription.subscription_id() + " receive",
      {{sc::kMessagingSystem, "gcp_pubsub"},
       {sc::kMessagingOperation, "receive"},
       {sc::kCodeFunction, "pubsub::SubscriberConnection::Pull"},
       {sc::kMessagingDestinationTemplate, subscription.FullName()}},
      options);
  return span;
}

StatusOr<pubsub::PullResponse> EndPullSpan(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> const& span,
    StatusOr<pubsub::PullResponse> response) {
  namespace sc = opentelemetry::trace::SemanticConventions;
  if (response.ok()) {
    auto message = response.value().message;
    span->SetAttribute(sc::kMessagingMessageId, message.message_id());
    if (!message.ordering_key().empty()) {
      span->SetAttribute("messaging.gcp_pubsub.message.ordering_key",
                         message.ordering_key());
    }
    span->SetAttribute(
        /*sc::kMessagingMessageEnvelopeSize=*/"messaging.message.envelope.size",
        static_cast<std::int64_t>(MessageSize(message)));
  }
  return internal::EndSpan(*span, std::move(response));
}

class SubscriberTracingConnection : public pubsub::SubscriberConnection {
 public:
  explicit SubscriberTracingConnection(
      std::shared_ptr<pubsub::SubscriberConnection> child_)
      : child_(std::move(child_)) {}

  ~SubscriberTracingConnection() override = default;

  future<Status> Subscribe(SubscribeParams p) override {
    return child_->Subscribe(p);
  };

  future<Status> ExactlyOnceSubscribe(ExactlyOnceSubscribeParams p) override {
    return child_->ExactlyOnceSubscribe(p);
  };

  StatusOr<pubsub::PullResponse> Pull() override {
    auto span = StartPullSpan();

    auto scope = opentelemetry::trace::Scope(span);
    return EndPullSpan(std::move(span), child_->Pull());
  };

  Options options() override { return child_->options(); };

 private:
  std::shared_ptr<pubsub::SubscriberConnection> child_;
};

}  // namespace

std::shared_ptr<pubsub::SubscriberConnection> MakeSubscriberTracingConnection(
    std::shared_ptr<pubsub::SubscriberConnection> connection) {
  return std::make_shared<SubscriberTracingConnection>(std::move(connection));
}

#else  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::shared_ptr<pubsub::SubscriberConnection> MakeSubscriberTracingConnection(
    std::shared_ptr<pubsub::SubscriberConnection> connection) {
  return connection;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
