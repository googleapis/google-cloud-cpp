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

#include "google/cloud/pubsub/internal/subscription_message_queue.h"
#include "google/cloud/pubsub/message.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

void SubscriptionMessageQueue::Start(MessageCallback cb) {
  std::lock_guard<std::mutex> lk(mu_);
  if (callback_) return;
  callback_ = std::move(cb);
}

void SubscriptionMessageQueue::Shutdown() {
  auto messages = [&] {
    std::unique_lock<std::mutex> lk(mu_);
    shutdown_ = true;
    auto messages = std::move(messages_);
    messages_.clear();
    return messages;
  }();

  if (messages.empty()) return;
  std::vector<std::string> ack_ids(messages.size());
  std::size_t total_size = 0;
  std::transform(messages.begin(), messages.end(), ack_ids.begin(),
                 [&](google::pubsub::v1::ReceivedMessage& m) {
                   total_size += MessageProtoSize(m.message());
                   return std::move(*m.mutable_ack_id());
                 });
  source_->BulkNack(std::move(ack_ids), total_size);
}

void SubscriptionMessageQueue::Read(std::size_t max_callbacks) {
  std::unique_lock<std::mutex> lk(mu_);
  if (!callback_) return;
  read_count_ += max_callbacks;
  DrainQueue(std::move(lk));
}

future<Status> SubscriptionMessageQueue::AckMessage(std::string const& ack_id,
                                                    std::size_t size) {
  return source_->AckMessage(ack_id, size);
}

future<Status> SubscriptionMessageQueue::NackMessage(std::string const& ack_id,
                                                     std::size_t size) {
  return source_->NackMessage(ack_id, size);
}

void SubscriptionMessageQueue::OnPull(google::pubsub::v1::PullResponse r) {
  std::unique_lock<std::mutex> lk(mu_);
  if (shutdown_) return;
  std::move(r.mutable_received_messages()->begin(),
            r.mutable_received_messages()->end(),
            std::back_inserter(messages_));
  DrainQueue(std::move(lk));
}

void SubscriptionMessageQueue::DrainQueue(std::unique_lock<std::mutex> lk) {
  while (!messages_.empty() && read_count_ > 0 && !shutdown_) {
    auto m = std::move(messages_.front());
    messages_.pop_front();
    --read_count_;
    // Don't hold a lock during the callback, as the callee may call `Read()`
    // or something similar.
    lk.unlock();
    callback_(std::move(m));
    lk.lock();
  }
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
