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

#include "google/cloud/pubsub/internal/pull_ack_handler_factory.h"
#include "google/cloud/pubsub/internal/tracing_pull_ack_handler.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/options.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

pubsub::PullAckHandler MakePullAckHandler(CompletionQueue cq,
                                          std::weak_ptr<SubscriberStub> stub,
                                          pubsub::Subscription subscription,
                                          std::string ack_id,
                                          std::int32_t delivery_attempt,
                                          Options const& options) {
  std::unique_ptr<pubsub::PullAckHandler::Impl> impl =
      std::make_unique<pubsub_internal::DefaultPullAckHandler>(
          std::move(cq), std::move(stub), options, std::move(subscription),
          std::move(ack_id), delivery_attempt);
  if (internal::TracingEnabled(options)) {
    impl = MakeTracingPullAckHandler(std::move(impl));
  }
  return pubsub::PullAckHandler(std::move(impl));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
