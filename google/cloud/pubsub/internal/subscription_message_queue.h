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
#include "google/cloud/internal/absl_flat_hash_map_quiet.h"
#include "google/cloud/internal/random.h"
#include <google/pubsub/v1/pubsub.pb.h>
#include <deque>
#include <functional>
#include <mutex>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * Keeps the queue of runnable messages.
 *
 * Recall that subscription message processing happens in stages, see
 * `SubscriptionSession` and go/cloud-cxx:pub-sub-subscriptions-dd for more
 * details.
 *
 * The next stage sets up a callback in `Start()` to receive messages from this
 * stage. This stage keeps a queue of messages ready to run, the next stage
 * drains the queue by calling `Read(n)` which allow this stage to send up to
 * `n` messages. After `n` messages are sent more calls to `Read(n)` *are*
 * required, the queue does not drain just because some messages completed.
 *
 * Messages with ordering keys are executed in order. The class keeps a message
 * queue per ordering key. The queue is created when a message with a new
 * ordering key is received. The queue is deleted when the last message with the
 * given ordering key is handled by the application (via the handler ack/nack
 * calls). Effectively this means that the presence of the queue serves as a
 * flag to block sending messages with the queue's ordering key to the next
 * stage.
 *
 * For messages with an ordering key, this class also maintains a mapping of
 * ack_id to ordering key. This is necessary to determine which ordering key
 * queue is drained when the message is acknowledged or rejected.
 */
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
  void AckMessage(std::string const& ack_id) override;
  void NackMessage(std::string const& ack_id) override;

 private:
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

  /// Process a nack() or ack() for a message
  void HandlerDone(std::string const& ack_id);

  std::shared_ptr<SessionShutdownManager> const shutdown_manager_;
  std::shared_ptr<SubscriptionBatchSource> const source_;

  using QueueByOrderingKey =
      absl::flat_hash_map<std::string,
                          std::deque<google::pubsub::v1::ReceivedMessage>>;

  std::mutex mu_;
  MessageCallback callback_;
  bool shutdown_ = false;
  std::size_t available_slots_ = 0;
  std::deque<google::pubsub::v1::ReceivedMessage> runnable_messages_;
  QueueByOrderingKey queues_;
  absl::flat_hash_map<std::string, std::string> ordering_key_by_ack_id_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_MESSAGE_QUEUE_H
