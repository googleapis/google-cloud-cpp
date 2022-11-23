// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_PULL_ACK_HANDLER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_PULL_ACK_HANDLER_H

#include "google/cloud/pubsub/backoff_policy.h"
#include "google/cloud/pubsub/exactly_once_ack_handler.h"
#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "absl/types/optional.h"
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class PullLeaseManager;

/**
 * Implements a `pubsub::ExactlyOnceAckHandler` suitable for blocking pull
 * requests.
 *
 * This is an implementation detail, hidden from the application.
 */
class PullAckHandler : public pubsub::ExactlyOnceAckHandler::Impl {
 public:
  PullAckHandler(CompletionQueue cq, std::weak_ptr<SubscriberStub> w,
                 Options options, pubsub::Subscription subscription,
                 std::string ack_id, std::int32_t delivery_attempt);
  ~PullAckHandler() override;

  future<Status> ack() override;
  future<Status> nack() override;
  std::int32_t delivery_attempt() const override;

 private:
  CompletionQueue cq_;
  std::weak_ptr<SubscriberStub> stub_;
  pubsub::Subscription subscription_;
  std::string ack_id_;
  std::int32_t delivery_attempt_;
  std::shared_ptr<PullLeaseManager> lease_manager_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_PULL_ACK_HANDLER_H
