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
      std::shared_ptr<SubscriptionBatchSource> child) {
    return std::shared_ptr<SubscriptionFlowControl>(new SubscriptionFlowControl(
        std::move(cq), std::move(shutdown_manager), std::move(child)));
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
      std::shared_ptr<SubscriptionBatchSource> child)
      : cq_(std::move(cq)),
        shutdown_manager_(std::move(shutdown_manager)),
        child_(std::move(child)),
        queue_(child_) {}

  void OnRead(StatusOr<google::pubsub::v1::StreamingPullResponse> r);

  google::cloud::CompletionQueue cq_;
  std::shared_ptr<SessionShutdownManager> const shutdown_manager_;
  std::shared_ptr<SubscriptionBatchSource> const child_;

  std::mutex mu_;
  SubscriptionMessageQueue queue_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_FLOW_CONTROL_H
