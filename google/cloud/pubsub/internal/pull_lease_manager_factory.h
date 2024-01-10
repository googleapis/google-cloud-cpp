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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_PULL_LEASE_MANAGER_FACTORY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_PULL_LEASE_MANAGER_FACTORY_H

#include "google/cloud/pubsub/internal/pull_lease_manager.h"
#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/internal/clock.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <chrono>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using Clock = ::google::cloud::internal::SystemClock;

std::shared_ptr<PullLeaseManager> MakePullLeaseManager(
    CompletionQueue cq, std::weak_ptr<SubscriberStub> stub,
    pubsub::Subscription subscription, std::string ack_id,
    Options const& options,
    std::shared_ptr<Clock> clock = std::make_shared<Clock>());

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_PULL_LEASE_MANAGER_FACTORY_H
