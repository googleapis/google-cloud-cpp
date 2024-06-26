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

#include "google/cloud/pubsub/internal/blocking_publisher_tracing_connection.h"
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/topic.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/status_or.h"
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/trace/scope.h>
#include <opentelemetry/trace/semantic_conventions.h>
#include <opentelemetry/trace/span_startoptions.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
namespace {

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> StartPublishSpan(
    pubsub::Topic const& topic, pubsub::Message const& m) {
  namespace sc = opentelemetry::trace::SemanticConventions;
  opentelemetry::trace::StartSpanOptions options;
  options.kind = opentelemetry::trace::SpanKind::kProducer;
  auto span = internal::MakeSpan(
      topic.topic_id() + " create",
      {{sc::kMessagingSystem, "gcp_pubsub"},
       {sc::kMessagingDestinationName, topic.topic_id()},
       {"gcp.project_id", topic.project_id()},
       {/*sc::kMessagingOperationType=*/"messaging.operation.type", "create"},
       {/*sc::kMessagingMessageEnvelopeSize=*/"messaging.message.envelope.size",
        static_cast<std::int64_t>(MessageSize(m))},
       {sc::kCodeFunction, "pubsub::BlockingPublisher::Publish"}},
      options);
  if (!m.ordering_key().empty()) {
    span->SetAttribute("messaging.gcp_pubsub.message.ordering_key",
                       m.ordering_key());
  }
  return span;
}

StatusOr<std::string> EndPublishSpan(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> const& span,
    StatusOr<std::string> message_id) {
  if (message_id) span->SetAttribute("messaging.message_id", *message_id);
  return internal::EndSpan(*span, std::move(message_id));
}

/**
 * A decorator that adds tracing for the BlockingPublisherTracingConnection.
 */
class BlockingPublisherTracingConnection
    : public pubsub::BlockingPublisherConnection {
 public:
  explicit BlockingPublisherTracingConnection(
      std::shared_ptr<pubsub::BlockingPublisherConnection> child)
      : child_(std::move(child)) {}

  StatusOr<std::string> Publish(PublishParams p) override {
    auto span = StartPublishSpan(p.topic, p.message);
    auto scope = opentelemetry::trace::Scope(span);
    return EndPublishSpan(std::move(span), child_->Publish(std::move(p)));
  }

  Options options() override { return child_->options(); };

 private:
  std::shared_ptr<pubsub::BlockingPublisherConnection> child_;
};

}  // namespace

std::shared_ptr<pubsub::BlockingPublisherConnection>
MakeBlockingPublisherTracingConnection(
    std::shared_ptr<pubsub::BlockingPublisherConnection> connection) {
  return std::make_shared<BlockingPublisherTracingConnection>(
      std::move(connection));
}

#else  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::shared_ptr<pubsub::BlockingPublisherConnection>
MakeBlockingPublisherTracingConnection(
    std::shared_ptr<pubsub::BlockingPublisherConnection> connection) {
  return connection;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
