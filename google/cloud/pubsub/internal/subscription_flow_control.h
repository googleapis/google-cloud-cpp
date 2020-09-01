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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_FLOW_CONTROL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_FLOW_CONTROL_H

#include "google/cloud/pubsub/internal/session_shutdown_manager.h"
#include "google/cloud/pubsub/internal/subscription_batch_source.h"
#include "google/cloud/pubsub/internal/subscription_message_queue.h"
#include "google/cloud/pubsub/version.h"
#include <functional>
#include <memory>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class SubscriptionFlowControl
    : public SubscriptionMessageSource,
      public std::enable_shared_from_this<SubscriptionFlowControl> {
 public:
  static std::shared_ptr<SubscriptionFlowControl> Create(
      google::cloud::CompletionQueue cq,
      std::shared_ptr<SessionShutdownManager> shutdown_manager,
      std::shared_ptr<SubscriptionBatchSource> child,
      std::size_t message_count_lwm, std::size_t message_count_hwm,
      std::size_t message_size_lwm, std::size_t message_size_hwm) {
    return std::shared_ptr<SubscriptionFlowControl>(new SubscriptionFlowControl(
        std::move(cq), std::move(shutdown_manager), std::move(child),
        message_count_lwm, message_count_hwm, message_size_lwm,
        message_size_hwm));
  }

  void Start(MessageCallback) override;
  void Shutdown() override;
  void Read(std::size_t max_callbacks) override;
  future<Status> AckMessage(std::string const& ack_id,
                            std::size_t size) override;
  future<Status> NackMessage(std::string const& ack_id,
                             std::size_t size) override;

 private:
  SubscriptionFlowControl(
      google::cloud::CompletionQueue cq,
      std::shared_ptr<SessionShutdownManager> shutdown_manager,
      std::shared_ptr<SubscriptionBatchSource> child,
      std::size_t message_count_lwm, std::size_t message_count_hwm,
      std::size_t message_size_lwm, std::size_t message_size_hwm)
      : cq_(std::move(cq)),
        shutdown_manager_(std::move(shutdown_manager)),
        child_(std::move(child)),
        message_count_lwm_(std::min(message_count_lwm, message_count_hwm)),
        message_count_hwm_(message_count_hwm),
        message_size_lwm_(std::min(message_size_lwm, message_size_hwm)),
        message_size_hwm_(message_size_hwm),
        queue_(child_) {}

  void MessageHandled(std::size_t size);
  void PullMore();
  void PullIfNeeded(std::unique_lock<std::mutex> lk);
  void OnPull(StatusOr<google::pubsub::v1::PullResponse> response,
              std::int32_t pull_message_count);

  std::size_t total_messages() const {
    return message_count_ + outstanding_pull_count_;
  }

  google::cloud::CompletionQueue cq_;
  std::shared_ptr<SessionShutdownManager> const shutdown_manager_;
  std::shared_ptr<SubscriptionBatchSource> const child_;
  std::size_t const message_count_lwm_;
  std::size_t const message_count_hwm_;
  std::size_t const message_size_lwm_;
  std::size_t const message_size_hwm_;

  std::mutex mu_;
  std::size_t message_count_ = 0;
  std::size_t message_size_ = 0;
  bool overflow_ = false;
  std::size_t outstanding_pull_count_ = 0;
  SubscriptionMessageQueue queue_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_FLOW_CONTROL_H
