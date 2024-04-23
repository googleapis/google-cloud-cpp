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

#include "google/cloud/pubsub/internal/pull_lease_manager_factory.h"
#include "google/cloud/pubsub/internal/default_pull_lease_manager.h"
#include "google/cloud/pubsub/internal/tracing_pull_lease_manager.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/options.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::shared_ptr<PullLeaseManager> MakePullLeaseManager(
    CompletionQueue cq, std::weak_ptr<SubscriberStub> stub,
    pubsub::Subscription subscription, std::string ack_id,
    Options const& options, std::shared_ptr<Clock> clock) {
  std::shared_ptr<PullLeaseManagerImpl> manager_impl =
      std::make_shared<pubsub_internal::DefaultPullLeaseManagerImpl>();
  if (internal::TracingEnabled(options)) {
    manager_impl = MakeTracingPullLeaseManagerImpl(std::move(manager_impl),
                                                   ack_id, subscription);
  }
  std::shared_ptr<PullLeaseManager> manager =
      std::make_shared<pubsub_internal::DefaultPullLeaseManager>(
          std::move(cq), std::move(stub), options, std::move(subscription),
          std::move(ack_id), std::move(manager_impl), std::move(clock));
  if (internal::TracingEnabled(options)) {
    manager = MakeTracingPullLeaseManager(std::move(manager));
  }
  return manager;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
