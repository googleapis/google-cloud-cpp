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
  std::unique_lock<std::mutex> lk(mu_);
  if (callback_) return;
  callback_ = std::move(cb);
  lk.unlock();
  auto weak = std::weak_ptr<SubscriptionMessageQueue>(shared_from_this());
  source_->Start([weak](StatusOr<google::pubsub::v1::StreamingPullResponse> r) {
    if (auto self = weak.lock()) self->OnRead(std::move(r));
  });
}

void SubscriptionMessageQueue::Shutdown() {
  Shutdown(std::unique_lock<std::mutex>(mu_));
  source_->Shutdown();
}

void SubscriptionMessageQueue::Read(std::size_t max_callbacks) {
  std::unique_lock<std::mutex> lk(mu_);
  if (!callback_) return;
  read_count_ += max_callbacks;
  DrainQueue(std::move(lk));
}

future<Status> SubscriptionMessageQueue::AckMessage(std::string const& ack_id,
                                                    std::size_t) {
  return source_->AckMessage(ack_id);
}

future<Status> SubscriptionMessageQueue::NackMessage(std::string const& ack_id,
                                                     std::size_t) {
  return source_->NackMessage(ack_id);
}

void SubscriptionMessageQueue::OnRead(
    StatusOr<google::pubsub::v1::StreamingPullResponse> r) {
  std::unique_lock<std::mutex> lk(mu_);
  if (!r) {
    shutdown_manager_->MarkAsShutdown(__func__, std::move(r).status());
    Shutdown(std::move(lk));
    return;
  }
  OnRead(std::move(lk), *std::move(r));
}

void SubscriptionMessageQueue::OnRead(
    std::unique_lock<std::mutex> lk,
    google::pubsub::v1::StreamingPullResponse r) {
  auto handle_response = [&] {
    shutdown_manager_->FinishedOperation("OnRead");
    std::move(r.mutable_received_messages()->begin(),
              r.mutable_received_messages()->end(),
              std::back_inserter(messages_));
    DrainQueue(std::move(lk));
  };
  auto bulk_nack = [&] {
    lk.unlock();
    std::vector<std::string> ack_ids(r.mutable_received_messages()->size());
    std::transform(r.mutable_received_messages()->begin(),
                   r.mutable_received_messages()->end(), ack_ids.begin(),
                   [&](google::pubsub::v1::ReceivedMessage& m) {
                     return std::move(*m.mutable_ack_id());
                   });
    (void)source_->BulkNack(std::move(ack_ids));
  };
  if (!shutdown_manager_->StartOperation(__func__, "OnRead", handle_response)) {
    bulk_nack();
  }
}

void SubscriptionMessageQueue::Shutdown(std::unique_lock<std::mutex> lk) {
  auto messages = [&] {
    shutdown_ = true;
    auto messages = std::move(messages_);
    messages_.clear();
    lk.unlock();
    return messages;
  }();

  if (messages.empty()) return;
  std::vector<std::string> ack_ids(messages.size());
  std::transform(messages.begin(), messages.end(), ack_ids.begin(),
                 [&](google::pubsub::v1::ReceivedMessage& m) {
                   return std::move(*m.mutable_ack_id());
                 });
  source_->BulkNack(std::move(ack_ids));
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
