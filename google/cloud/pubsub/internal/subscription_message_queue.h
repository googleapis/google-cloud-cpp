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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_MESSAGE_QUEUE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_MESSAGE_QUEUE_H

#include "google/cloud/pubsub/internal/session_shutdown_manager.h"
#include "google/cloud/pubsub/internal/subscription_batch_source.h"
#include "google/cloud/pubsub/internal/subscription_message_source.h"
#include "google/cloud/pubsub/version.h"
#include <google/pubsub/v1/pubsub.pb.h>
#include <deque>
#include <functional>
#include <mutex>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class SubscriptionMessageQueue : public SubscriptionMessageSource {
 public:
  explicit SubscriptionMessageQueue(
      std::shared_ptr<SubscriptionBatchSource> source)
      : source_(std::move(source)) {}

  void Start(MessageCallback cb) override;
  void Shutdown() override;
  void Read(std::size_t max_callbacks) override;
  future<Status> AckMessage(std::string const& ack_id,
                            std::size_t size) override;
  future<Status> NackMessage(std::string const& ack_id,
                             std::size_t size) override;

  void OnPull(google::pubsub::v1::PullResponse r);

 private:
  void DrainQueue(std::unique_lock<std::mutex> lk);

  std::shared_ptr<SubscriptionBatchSource> const source_;

  std::mutex mu_;
  MessageCallback callback_;
  std::size_t read_count_ = 0;
  std::deque<google::pubsub::v1::ReceivedMessage> messages_;
  bool shutdown_ = false;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_MESSAGE_QUEUE_H
