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

#include "google/cloud/pubsub/internal/message_callback.h"
#include "google/cloud/pubsub/internal/tracing_exactly_once_ack_handler.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/opentelemetry.h"
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/trace/semantic_conventions.h>
#include <opentelemetry/trace/span_startoptions.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace {

class TracingMessageCallback : public MessageCallback {
 public:
  explicit TracingMessageCallback(std::shared_ptr<MessageCallback> child,
                                  Options const& opts)
      : child_(std::move(child)),
        subscription_id_(
            opts.get<pubsub::SubscriptionOption>().subscription_id()) {}

  ~TracingMessageCallback() override = default;

  void user_callback(MessageAndHandler m) override {
    namespace sc = opentelemetry::trace::SemanticConventions;
    opentelemetry::trace::StartSpanOptions options;
    if (m.subscribe_span.span) {
      options.parent = m.subscribe_span.span->GetContext();
    }
    auto span =
        internal::MakeSpan(subscription_id_ + " process",
                           {{sc::kMessagingSystem, "gcp_pubsub"}}, options);
    m.ack_handler = MakeTracingExactlyOnceAckHandler(std::move(m.ack_handler),
                                                     m.subscribe_span);
    child_->user_callback(std::move(m));
    span->End();
  }

  std::shared_ptr<MessageCallback> child_;
  std::string subscription_id_;
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> subscribe_span_;
};

}  // namespace

std::shared_ptr<MessageCallback> MakeTracingMessageCallback(
    std::shared_ptr<MessageCallback> message_callback, Options const& opts) {
  return std::make_shared<TracingMessageCallback>(std::move(message_callback),
                                                  opts);
}

#else  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::shared_ptr<MessageCallback> MakeTracingMessageCallback(
    std::shared_ptr<MessageCallback> message_callback, Options const&) {
  return message_callback;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
