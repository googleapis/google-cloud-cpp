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

#include "google/cloud/pubsub/internal/flow_controlled_publisher_tracing_connection.h"
#include "google/cloud/pubsub/publisher_connection.h"
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/internal/opentelemetry.h"
#include <opentelemetry/trace/context.h>
#include <opentelemetry/trace/semantic_conventions.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace {
class FlowControlledPublisherTracingConnection
    : public pubsub::PublisherConnection {
 public:
  explicit FlowControlledPublisherTracingConnection(
      std::shared_ptr<pubsub::PublisherConnection> child)
      : child_(std::move(child)) {}

  ~FlowControlledPublisherTracingConnection() override = default;

  future<StatusOr<std::string>> Publish(PublishParams p) override {
    namespace sc = ::opentelemetry::trace::SemanticConventions;
    auto span = internal::MakeSpan(
        "publisher flow control",
        {{sc::kCodeFunction,
          "pubsub::FlowControlledPublisherConnection::Publish"}});
    auto result = child_->Publish(std::move(p));
    internal::EndSpan(*span);
    return result;
  }

  void Flush(FlushParams p) override {
    auto span =
        internal::MakeSpan("pubsub::FlowControlledPublisherConnection::Flush");
    child_->Flush(std::move(p));
    internal::EndSpan(*span);
  }

  void ResumePublish(ResumePublishParams p) override {
    auto span = internal::MakeSpan(
        "pubsub::FlowControlledPublisherConnection::ResumePublish");
    child_->ResumePublish(std::move(p));
    internal::EndSpan(*span);
  }

 private:
  std::shared_ptr<pubsub::PublisherConnection> child_;
};

}  // namespace

std::shared_ptr<pubsub::PublisherConnection>
MakeFlowControlledPublisherTracingConnection(
    std::shared_ptr<pubsub::PublisherConnection> connection) {
  return std::make_shared<FlowControlledPublisherTracingConnection>(
      std::move(connection));
}

#else  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::shared_ptr<pubsub::PublisherConnection>
MakeFlowControlledPublisherTracingConnection(
    std::shared_ptr<pubsub::PublisherConnection> connection) {
  return connection;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
