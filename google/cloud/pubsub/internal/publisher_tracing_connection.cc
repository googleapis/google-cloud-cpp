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

#include "google/cloud/pubsub/internal/publisher_tracing_connection.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/future.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include <memory>
#include <string>
#include <utility>
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/status_or.h"
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/trace/scope.h>
#include <opentelemetry/trace/semantic_conventions.h>
#include <opentelemetry/trace/span_metadata.h>
#include <opentelemetry/trace/span_startoptions.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
namespace {

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> StartPublishSpan(
    std::string const& topic, pubsub::Message const& m) {
  namespace sc = opentelemetry::trace::SemanticConventions;
  opentelemetry::trace::StartSpanOptions options;
  options.kind = opentelemetry::trace::SpanKind::kProducer;
  auto span =
      internal::GetTracer(internal::CurrentOptions())
          ->StartSpan(topic + " send",
                      {
                          {sc::kMessagingSystem, "pubsub"},
                          {sc::kMessagingDestinationName, topic},
                          {sc::kMessagingDestinationTemplate, "topic"},
                          {"messaging.pubsub.ordering_key", m.ordering_key()},
                          {"messaging.message.total_size_bytes",
                           static_cast<int>(MessageSize(m))},
                      },
                      options);
  return span;
}

future<StatusOr<std::string>> EndPublishSpan(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
    future<StatusOr<std::string>> f) {
  return f.then([span = std::move(span)](auto fut) {
    auto message_id = fut.get();
    if (message_id) span->SetAttribute("messaging.message_id", *message_id);
    return internal::EndSpan(*span, std::move(message_id));
  });
}

/**
 * A decorator that adds tracing for the PublisherConnection.
 */
class PublisherTracingConnection : public pubsub::PublisherConnection {
 public:
  explicit PublisherTracingConnection(
      pubsub::Topic topic, std::shared_ptr<pubsub::PublisherConnection> child)
      : topic_(std::move(topic)), child_(std::move(child)) {}

  ~PublisherTracingConnection() override = default;

  future<StatusOr<std::string>> Publish(PublishParams p) override {
    auto span = StartPublishSpan(topic_.FullName(), p.message);
    auto scope = opentelemetry::trace::Scope(span);
    return EndPublishSpan(std::move(span), child_->Publish(std::move(p)));
  };

  void Flush(FlushParams p) override {
    auto span = internal::MakeSpan("pubsub::Publisher::Flush");
    auto scope = opentelemetry::trace::Scope(span);
    child_->Flush(std::move(p));
    internal::EndSpan(*span);
  }

  void ResumePublish(ResumePublishParams p) override {
    auto span = internal::MakeSpan("pubsub::Publisher::ResumePublish");
    auto scope = opentelemetry::trace::Scope(span);
    child_->ResumePublish(std::move(p));
    internal::EndSpan(*span);
  };

 private:
  pubsub::Topic const topic_;
  std::shared_ptr<pubsub::PublisherConnection> child_;
};

}  // namespace

std::shared_ptr<pubsub::PublisherConnection> MakePublisherTracingConnection(
    pubsub::Topic topic,
    std::shared_ptr<pubsub::PublisherConnection> connection) {
  return std::make_shared<PublisherTracingConnection>(std::move(topic),
                                                      std::move(connection));
}

#else  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::shared_ptr<pubsub::PublisherConnection> MakePublisherTracingConnection(
    pubsub::Topic, std::shared_ptr<pubsub::PublisherConnection> connection) {
  return connection;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
