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

#include "google/cloud/pubsub/internal/subscription_session.h"
#include "google/cloud/log.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {
future<Status> CreateSubscriptionSessionImpl(
    google::cloud::CompletionQueue const& executor,
    std::shared_ptr<pubsub_internal::SessionShutdownManager> const&
        shutdown_manager,
    std::shared_ptr<pubsub_internal::SubscriptionBatchSource> source,
    pubsub::SubscriberConnection::SubscribeParams p) {
  auto flow_control = SubscriptionFlowControl::Create(
      executor, shutdown_manager, std::move(source),
      p.options.message_count_lwm(), p.options.message_count_hwm());
  auto concurrency_control = SubscriptionConcurrencyControl::Create(
      executor, shutdown_manager, std::move(flow_control),
      p.options.concurrency_lwm(), p.options.concurrency_hwm());

  auto result = shutdown_manager->Start(promise<Status>(
      [concurrency_control] { concurrency_control->Shutdown(); }));
  concurrency_control->Start(std::move(p.callback));
  return result;
}
}  // namespace

future<Status> CreateSubscriptionSession(
    std::shared_ptr<pubsub_internal::SubscriberStub> const& stub,
    google::cloud::CompletionQueue const& executor,
    pubsub::SubscriberConnection::SubscribeParams p) {
  auto shutdown_manager = std::make_shared<SessionShutdownManager>();
  auto lease_management = SubscriptionLeaseManagement::Create(
      executor, shutdown_manager, stub, std::move(p.full_subscription_name),
      p.options.max_deadline_time());

  return CreateSubscriptionSessionImpl(executor, std::move(shutdown_manager),
                                       std::move(lease_management),
                                       std::move(p));
}

future<Status> CreateTestingSubscriptionSession(
    std::shared_ptr<pubsub_internal::SubscriberStub> const& stub,
    google::cloud::CompletionQueue const& executor,
    pubsub::SubscriberConnection::SubscribeParams p) {
  auto shutdown_manager = std::make_shared<SessionShutdownManager>();

  auto cq = executor;  // need a copy to make it mutable
  auto timer = [cq](std::chrono::system_clock::time_point) mutable {
    return cq.MakeRelativeTimer(std::chrono::milliseconds(50))
        .then([](future<StatusOr<std::chrono::system_clock::time_point>> f) {
          return f.get().status();
        });
  };
  auto lease_management = SubscriptionLeaseManagement::CreateForTesting(
      executor, shutdown_manager, timer, stub,
      std::move(p.full_subscription_name), p.options.max_deadline_time());

  return CreateSubscriptionSessionImpl(executor, std::move(shutdown_manager),
                                       std::move(lease_management),
                                       std::move(p));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
