// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_SESSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_SESSION_H

#include "google/cloud/pubsub/application_callback.h"
#include "google/cloud/pubsub/backoff_policy.h"
#include "google/cloud/pubsub/internal/session_shutdown_manager.h"
#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/internal/subscription_concurrency_control.h"
#include "google/cloud/pubsub/retry_policy.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/status_or.h"
#include <deque>
#include <memory>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// TODO(#9327) - move this to application_callback.h
/**
 * Defines the interface for application-level callbacks with exactly-once
 * delivery.
 *
 * Applications provide a callable compatible with this type to receive
 * messages.  They acknowledge (or reject) messages using
 * `ExactlyOnceAckHandler`. This is a move-only type to support asynchronously
 * acknowledgments.
 */
using ExactlyOnceApplicationCallback =
    std::function<void(pubsub::Message, ExactlyOnceAckHandler)>;

future<Status> CreateSubscriptionSession(
    pubsub::Subscription const& subscription, Options const& opts,
    std::shared_ptr<SubscriberStub> const& stub, CompletionQueue const& cq,
    std::string client_id, pubsub::ApplicationCallback application_callback);

future<Status> CreateSubscriptionSession(
    pubsub::Subscription const& subscription, Options const& opts,
    std::shared_ptr<SubscriberStub> const& stub, CompletionQueue const& cq,
    std::string client_id, ExactlyOnceApplicationCallback application_callback);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_SESSION_H
