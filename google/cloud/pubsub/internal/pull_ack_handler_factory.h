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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_PULL_ACK_HANDLER_FACTORY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_PULL_ACK_HANDLER_FACTORY_H

#include "google/cloud/pubsub/internal/default_pull_ack_handler.h"
#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/pull_ack_handler.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
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
                                          Options const& options);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_PULL_ACK_HANDLER_FACTORY_H
