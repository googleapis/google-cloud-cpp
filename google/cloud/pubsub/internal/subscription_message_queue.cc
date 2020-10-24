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
      auto const ordered = !key.empty();
      auto& queue = queues_[key];
      queue.messages.push_back(std::move(m));
      // Queues require ordering are runnable only if there are no running
      // messages
      if (!ordered || queue.running == 0) {
        runnable_queues_.insert(std::move(key));
      }
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
  auto ack_ids = [&](std::unique_lock<std::mutex>) {
    std::vector<std::string> ack_ids;
    for (auto& kv : queues_) {
      for (auto& m : kv.second.messages) {
        ack_ids.push_back(std::move(*m.mutable_ack_id()));
      }
    }
    queues_.clear();
    return ack_ids;
  }(std::move(lk));

  if (ack_ids.empty()) return;
  source_->BulkNack(std::move(ack_ids));
}

void SubscriptionMessageQueue::DrainQueue(std::unique_lock<std::mutex> lk) {
  while (KeepDraining(lk)) {
    auto m = PickFromRunnable(lk);
    if (!m) continue;
    // Don't hold a lock during the callback, as the callee may call `Read()`
    // or something similar.
    lk.unlock();
    callback_(std::move(*m));
    lk.lock();
  }
}

absl::optional<google::pubsub::v1::ReceivedMessage>
SubscriptionMessageQueue::PickFromRunnable(
    std::unique_lock<std::mutex> const&) {
  // Pick one of the runnable queues at random, and select the next element
  // from that queue.
  auto idx = std::uniform_int_distribution<std::size_t>(
      0, runnable_queues_.size() - 1)(generator_);
  auto r = runnable_queues_.begin();
  for (std::size_t i = 0; i != idx; ++i) ++r;  // std::advance() did not work
  auto key = *r;
  auto const ordered = !key.empty();
  auto ql = queues_.find(key);
  if (ql == queues_.end()) return {};
  auto& q = ql->second;
  if (q.messages.empty()) {
    runnable_queues_.erase(r);
    return {};
  }
  auto m = std::move(q.messages.front());
  ordering_key_by_ack_id_[m.ack_id()] = std::move(key);
  q.messages.pop_front();
  ++q.running;
  --available_slots_;
  if (q.messages.empty() || ordered) {
    // The queue is no longer runnable, remove it from consideration.
    runnable_queues_.erase(r);
  }
  return m;
}

void SubscriptionMessageQueue::HandlerDone(std::string const& ack_id) {
  std::unique_lock<std::mutex> lk(mu_);
  auto loc = ordering_key_by_ack_id_.find(ack_id);
  if (loc == ordering_key_by_ack_id_.end()) return;
  auto key = std::move(loc->second);
  ordering_key_by_ack_id_.erase(loc);
  auto& q = queues_[key];
  if (--q.running == 0 && !q.messages.empty()) {
    runnable_queues_.insert(std::move(key));
  }
  DrainQueue(std::move(lk));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
