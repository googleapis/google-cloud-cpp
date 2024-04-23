// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_PULL_LEASE_MANAGER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_PULL_LEASE_MANAGER_H

#include "google/cloud/pubsub/internal/pull_lease_manager.h"
#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include <gmock/gmock.h>
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A class to mock pubsub_internal::PullLeaseManager
 */
class MockPullLeaseManager : public pubsub_internal::PullLeaseManager {
 public:
  MOCK_METHOD(void, StartLeaseLoop, (), (override));
  MOCK_METHOD(std::chrono::milliseconds, LeaseRefreshPeriod, (),
              (const, override));
  MOCK_METHOD(future<Status>, ExtendLease,
              (std::shared_ptr<pubsub_internal::SubscriberStub> stub,
               std::chrono::system_clock::time_point time_now,
               std::chrono::seconds extension),
              (override));
  MOCK_METHOD(std::string, ack_id, (), (const, override));
  MOCK_METHOD(pubsub::Subscription, subscription, (), (const, override));
};

class MockPullLeaseManagerImpl : public pubsub_internal::PullLeaseManagerImpl {
 public:
  MOCK_METHOD(future<Status>, AsyncModifyAckDeadline,
              (std::shared_ptr<pubsub_internal::SubscriberStub> stub,
               google::cloud::CompletionQueue& cq,
               std::shared_ptr<grpc::ClientContext> context,
               google::cloud::internal::ImmutableOptions options,
               google::pubsub::v1::ModifyAckDeadlineRequest const& request),
              (override));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_PULL_LEASE_MANAGER_H
