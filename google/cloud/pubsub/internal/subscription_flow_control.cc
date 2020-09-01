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

#include "google/cloud/pubsub/internal/subscription_flow_control.h"
#include "google/cloud/pubsub/message.h"
#include <numeric>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

void SubscriptionFlowControl::Start(MessageCallback cb) {
  std::unique_lock<std::mutex> lk(mu_);
  queue_.Start(std::move(cb));
  lk.unlock();
  auto self = shared_from_this();
  shutdown_manager_->StartAsyncOperation(__func__, "PullMore", cq_,
                                         [self] { self->PullMore(); });
}

void SubscriptionFlowControl::Shutdown() {
  queue_.Shutdown();
  child_->Shutdown();
}

void SubscriptionFlowControl::Read(std::size_t max_callbacks) {
  queue_.Read(max_callbacks);
}

future<Status> SubscriptionFlowControl::AckMessage(std::string const& ack_id,
                                                   std::size_t size) {
  auto result = queue_.AckMessage(ack_id, size);
  MessageHandled(size);
  return result;
}

future<Status> SubscriptionFlowControl::NackMessage(std::string const& ack_id,
                                                    std::size_t size) {
  auto result = queue_.NackMessage(ack_id, size);
  MessageHandled(size);
  return result;
}

void SubscriptionFlowControl::MessageHandled(std::size_t size) {
  std::unique_lock<std::mutex> lk(mu_);
  if (message_count_ != 0) --message_count_;
  message_size_ -= (std::min)(message_size_, size);
  if (message_count_ <= message_count_lwm_ &&
      message_size_ <= message_size_lwm_) {
    overflow_ = false;
  }
  auto self = shared_from_this();
  shutdown_manager_->StartAsyncOperation(__func__, "PullMore", cq_,
                                         [self] { self->PullMore(); });
}

void SubscriptionFlowControl::PullMore() {
  if (shutdown_manager_->FinishedOperation("PullMore")) return;
  PullIfNeeded(std::unique_lock<std::mutex>(mu_));
}

void SubscriptionFlowControl::PullIfNeeded(std::unique_lock<std::mutex> lk) {
  if (overflow_ || total_messages() >= message_count_hwm_) return;

  auto const maximum_messages =
      static_cast<std::int32_t>(message_count_hwm_ - total_messages());

  shutdown_manager_->StartOperation(__func__, "OnPull", [&] {
    auto weak = std::weak_ptr<SubscriptionFlowControl>(shared_from_this());
    outstanding_pull_count_ += maximum_messages;
    lk.unlock();
    child_->Pull(maximum_messages)
        .then([weak, maximum_messages](
                  future<StatusOr<google::pubsub::v1::PullResponse>> f) {
          if (auto self = weak.lock()) self->OnPull(f.get(), maximum_messages);
        });
  });
}

void SubscriptionFlowControl::OnPull(
    StatusOr<google::pubsub::v1::PullResponse> response,
    std::int32_t pull_message_count) {
  std::unique_lock<std::mutex> lk(mu_);
  outstanding_pull_count_ -=
      std::min<std::size_t>(pull_message_count, outstanding_pull_count_);
  if (!response) {
    shutdown_manager_->FinishedOperation("OnPull");
    shutdown_manager_->MarkAsShutdown(__func__, std::move(response).status());
    Shutdown();
    return;
  }
  if (shutdown_manager_->FinishedOperation("OnPull")) {
    std::vector<std::string> ack_ids(
        response->mutable_received_messages()->size());
    std::size_t total_size = 0;
    std::transform(response->mutable_received_messages()->begin(),
                   response->mutable_received_messages()->end(),
                   ack_ids.begin(),
                   [&](google::pubsub::v1::ReceivedMessage& m) {
                     total_size += MessageProtoSize(m.message());
                     return std::move(*m.mutable_ack_id());
                   });
    (void)child_->BulkNack(std::move(ack_ids), total_size);
    return;
  }
  message_count_ += response->received_messages_size();
  message_size_ += std::accumulate(
      response->received_messages().begin(),
      response->received_messages().end(), std::size_t{0},
      [](std::size_t a, google::pubsub::v1::ReceivedMessage const& m) {
        return a + MessageProtoSize(m.message());
      });
  if (message_count_ >= message_count_hwm_ ||
      message_size_ >= message_size_hwm_) {
    overflow_ = true;
  }
  lk.unlock();
  auto self = shared_from_this();
  shutdown_manager_->StartAsyncOperation(__func__, "PullMore", cq_,
                                         [self] { self->PullMore(); });
  queue_.OnPull(*std::move(response));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
