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
#include "google/cloud/internal/random.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include <google/pubsub/v1/pubsub.pb.h>
#include <deque>
#include <functional>
#include <mutex>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class SubscriptionMessageQueue
    : public SubscriptionMessageSource,
      public std::enable_shared_from_this<SubscriptionMessageQueue> {
 public:
  static std::shared_ptr<SubscriptionMessageQueue> Create(
      std::shared_ptr<SessionShutdownManager> shutdown_manager,
      std::shared_ptr<SubscriptionBatchSource> source) {
    return std::shared_ptr<SubscriptionMessageQueue>(
        new SubscriptionMessageQueue(std::move(shutdown_manager),
                                     std::move(source)));
  }

  void Start(MessageCallback cb) override;
  void Shutdown() override;
  void Read(std::size_t max_callbacks) override;
  future<Status> AckMessage(std::string const& ack_id,
                            std::size_t size) override;
  future<Status> NackMessage(std::string const& ack_id,
                             std::size_t size) override;

 private:
  // This class keeps a collection of queues indexed by ordering key, using
  // the empty string to index the queue of messages without an ordering key.
  // A queue with a real ordering key is is ready if (a) it is not empty, and
  // (b) there are no other messages for that ordering key being processed.
  //
  // When the next stage requests more messages we pull from the ready queues
  // first, one message at a time, picking the queue at random, until the next
  // stage has no more room. Because the queues with ordering keys become
  // immediately not-ready they are blocked after one message.
  //
  // [1]: the messages without an ordering key are indexed with an empty string.
  using QueueByOrderingKey = absl::flat_hash_map<std::string, std::deque<google::pubsub::v1::ReceivedMessage>>;

  explicit SubscriptionMessageQueue(
      std::shared_ptr<SessionShutdownManager> shutdown_manager,
      std::shared_ptr<SubscriptionBatchSource> source)
      : shutdown_manager_(std::move(shutdown_manager)),
        source_(std::move(source)) {}

  void OnRead(StatusOr<google::pubsub::v1::StreamingPullResponse> r);
  void OnRead(std::unique_lock<std::mutex> lk,
              google::pubsub::v1::StreamingPullResponse r);
  void Shutdown(std::unique_lock<std::mutex> lk);
  void DrainQueue(std::unique_lock<std::mutex> lk);
  bool KeepDraining(std::unique_lock<std::mutex> const&) {
    return !runnable_messages_.empty() && available_slots_ > 0 && !shutdown_;
  }
  absl::optional<google::pubsub::v1::ReceivedMessage> PickFromRunnable(
      std::unique_lock<std::mutex> const&);

  /// Process a nack() or ack() for a message
  void HandlerDone(std::string const& ack_id);

  std::shared_ptr<SessionShutdownManager> const shutdown_manager_;
  std::shared_ptr<SubscriptionBatchSource> const source_;

  std::mutex mu_;
  MessageCallback callback_;
  bool shutdown_ = false;
  std::size_t available_slots_ = 0;
  std::deque<google::pubsub::v1::ReceivedMessage> runnable_messages_;
  QueueByOrderingKey queues_;
  absl::flat_hash_map<std::string, std::string> ordering_key_by_ack_id_;
  google::cloud::internal::DefaultPRNG generator_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_MESSAGE_QUEUE_H
