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
#include "google/cloud/pubsub/publisher_connection.h"
#include <memory>
#include <string>

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/future.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/status_or.h"
#include <opentelemetry/trace/scope.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

future<StatusOr<std::string>> PublisherTracingConnection::Publish(
    PublishParams p) {
  // TODO(#12525): Add span for publish.
  return child_->Publish(std::move(p));
}

void PublisherTracingConnection::Flush(FlushParams p) {
  auto span = internal::MakeSpan("pubsub::Publisher::Flush");
  auto scope = opentelemetry::trace::Scope(span);
  child_->Flush(std::move(p));
  internal::EndSpan(*span);
}

void PublisherTracingConnection::ResumePublish(ResumePublishParams p) {
  auto span = internal::MakeSpan("pubsub::Publisher::ResumePublish");
  auto scope = opentelemetry::trace::Scope(span);
  child_->ResumePublish(std::move(p));
  internal::EndSpan(*span);
}

std::shared_ptr<pubsub::PublisherConnection> MakePublisherTracingConnection(
    std::shared_ptr<pubsub::PublisherConnection> connection) {
  return std::make_shared<PublisherTracingConnection>(std::move(connection));
}

#else  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::shared_ptr<pubsub::PublisherConnection> MakePublisherTracingConnection(
    std::shared_ptr<pubsub::PublisherConnection> connection) {
  return connection;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
