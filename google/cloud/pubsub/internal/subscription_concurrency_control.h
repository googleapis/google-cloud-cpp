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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_CONCURRENCY_CONTROL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_CONCURRENCY_CONTROL_H

#include "google/cloud/pubsub/application_callback.h"
#include "google/cloud/pubsub/internal/session_shutdown_manager.h"
#include "google/cloud/pubsub/internal/subscription_message_source.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/version.h"
#include <chrono>
#include <memory>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class SubscriptionConcurrencyControl
    : public std::enable_shared_from_this<SubscriptionConcurrencyControl> {
 public:
  static std::shared_ptr<SubscriptionConcurrencyControl> Create(
      google::cloud::CompletionQueue cq,
      std::shared_ptr<SessionShutdownManager> shutdown_manager,
      std::shared_ptr<SubscriptionMessageSource> source,
      std::size_t max_concurrency) {
    return std::shared_ptr<SubscriptionConcurrencyControl>(
        new SubscriptionConcurrencyControl(std::move(cq),
                                           std::move(shutdown_manager),
                                           std::move(source), max_concurrency));
  }

  void Start(pubsub::ApplicationCallback);
  void Shutdown();
  void AckMessage(std::string const& ack_id);
  void NackMessage(std::string const& ack_id);

 private:
  SubscriptionConcurrencyControl(
      google::cloud::CompletionQueue cq,
      std::shared_ptr<SessionShutdownManager> shutdown_manager,
      std::shared_ptr<SubscriptionMessageSource> source,
      std::size_t max_concurrency)
      : cq_(std::move(cq)),
        shutdown_manager_(std::move(shutdown_manager)),
        source_(std::move(source)),
        max_concurrency_(max_concurrency) {}

  void MessageHandled();
  void OnMessage(google::pubsub::v1::ReceivedMessage m);
  void OnMessageAsync(google::pubsub::v1::ReceivedMessage m,
                      std::weak_ptr<SubscriptionConcurrencyControl> w);

  std::size_t total_messages() const {
    return message_count_ + messages_requested_;
  }

  google::cloud::CompletionQueue cq_;
  std::shared_ptr<SessionShutdownManager> const shutdown_manager_;
  std::shared_ptr<SubscriptionMessageSource> const source_;
  std::size_t const max_concurrency_;

  std::mutex mu_;
  pubsub::ApplicationCallback callback_;
  std::size_t message_count_ = 0;
  std::size_t messages_requested_ = 0;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_CONCURRENCY_CONTROL_H
