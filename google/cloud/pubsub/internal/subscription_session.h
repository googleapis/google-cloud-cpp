// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_SESSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_SESSION_H

#include "google/cloud/pubsub/backoff_policy.h"
#include "google/cloud/pubsub/internal/session_shutdown_manager.h"
#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/internal/subscription_concurrency_control.h"
#include "google/cloud/pubsub/internal/subscription_flow_control.h"
#include "google/cloud/pubsub/internal/subscription_lease_management.h"
#include "google/cloud/pubsub/retry_policy.h"
#include "google/cloud/pubsub/subscriber_connection.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/status_or.h"
#include <deque>
#include <memory>
#include <mutex>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

future<Status> CreateSubscriptionSession(
    std::shared_ptr<pubsub_internal::SubscriberStub> const& stub,
    google::cloud::CompletionQueue const& executor,
    pubsub::SubscriberConnection::SubscribeParams p,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy);

future<Status> CreateTestingSubscriptionSession(
    std::shared_ptr<pubsub_internal::SubscriberStub> const& stub,
    google::cloud::CompletionQueue const& executor,
    pubsub::SubscriberConnection::SubscribeParams p,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy = {},
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy = {});

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_SESSION_H
