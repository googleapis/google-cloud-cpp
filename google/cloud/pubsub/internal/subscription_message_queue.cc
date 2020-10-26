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
  available_slots_ += max_callbacks;
  DrainQueue(std::move(lk));
}

future<Status> SubscriptionMessageQueue::AckMessage(std::string const& ack_id,
                                                    std::size_t) {
  HandlerDone(ack_id);
  source_->AckMessage(ack_id);
  return make_ready_future(Status{});
}

future<Status> SubscriptionMessageQueue::NackMessage(std::string const& ack_id,
                                                     std::size_t) {
  HandlerDone(ack_id);
  source_->NackMessage(ack_id);
  return make_ready_future(Status{});
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
    for (auto& m : *r.mutable_received_messages()) {
      auto key = m.message().ordering_key();
      if (key.empty()) {
        // Empty key, requires no ordering and therefore immediately runnable.
        runnable_messages_.push_back(std::move(m));
        continue;
      }
      // The message requires ordering, find out if there is an existing queue
      // for its ordering key, and insert one if necessary.
      auto loc = queues_.insert({key, {}});
      // There is no queue for this ordering key, that means no other messages
      // are present, we can push the message to the runnable queue. We leave
      // the per-ordering-key queue as a marker for any other incoming messages
      // with the same ordering key.
      if (loc.second) {
        runnable_messages_.push_back(std::move(m));
        continue;
      }
      // Insert the messages into the existing queue.
      loc.first->second.push_back(std::move(m));
    }
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
  shutdown_ = true;
  available_slots_ = 0;
  QueueByOrderingKey queues;
  queues.swap(queues_);
  std::deque<google::pubsub::v1::ReceivedMessage> runnable_messages;
  runnable_messages.swap(runnable_messages_);
  lk.unlock();

  std::vector<std::string> ack_ids;
  for (auto& kv : queues) {
    for (auto& m : kv.second) {
      ack_ids.push_back(std::move(*m.mutable_ack_id()));
    }
  }
  for (auto& m : runnable_messages) {
    ack_ids.push_back(std::move(*m.mutable_ack_id()));
  }

  if (ack_ids.empty()) return;
  source_->BulkNack(std::move(ack_ids));
}

void SubscriptionMessageQueue::DrainQueue(std::unique_lock<std::mutex> lk) {
  while (!runnable_messages_.empty() && available_slots_ > 0 && !shutdown_) {
    auto m = std::move(runnable_messages_.front());
    runnable_messages_.pop_front();
    --available_slots_;
    // No need to track messages without an ordering key, as there is no action
    // to take in their HandlerDone() member function.
    if (!m.message().ordering_key().empty()) {
      ordering_key_by_ack_id_[m.ack_id()] = m.message().ordering_key();
    }
    // Don't hold a lock during the callback, as the callee may call `Read()`
    // or something similar.
    lk.unlock();
    callback_(std::move(m));
    lk.lock();
  }
}

void SubscriptionMessageQueue::HandlerDone(std::string const& ack_id) {
  std::unique_lock<std::mutex> lk(mu_);
  // Find out the ordering key for this message.
  auto loc = ordering_key_by_ack_id_.find(ack_id);
  // Messages without an ordering key are not inserted in the collection (see
  // `DrainQueue()`), so this happens routinely.
  if (loc == ordering_key_by_ack_id_.end()) return;
  auto key = std::move(loc->second);
  ordering_key_by_ack_id_.erase(loc);
  auto ql = queues_.find(key);
  // This is purely defensive, but should not happen.
  if (ql == queues_.end()) return;
  if (ql->second.empty()) {
    // There are no more messages for this ordering key, remove the queue, as it
    // also serves as a marker to order the next message.
    queues_.erase(ql);
    return;
  }
  auto m = std::move(ql->second.front());
  ql->second.pop_front();
  runnable_messages_.push_back(std::move(m));
  DrainQueue(std::move(lk));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
