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
  auto weak = std::weak_ptr<SubscriptionFlowControl>(shared_from_this());
  child_->Start([weak](StatusOr<google::pubsub::v1::StreamingPullResponse> r) {
    if (auto self = weak.lock()) self->OnRead(std::move(r));
  });
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
  return queue_.AckMessage(ack_id, size);
}

future<Status> SubscriptionFlowControl::NackMessage(std::string const& ack_id,
                                                    std::size_t size) {
  return queue_.NackMessage(ack_id, size);
}

void SubscriptionFlowControl::OnRead(
    StatusOr<google::pubsub::v1::StreamingPullResponse> r) {
  std::unique_lock<std::mutex> lk(mu_);
  if (!r) {
    shutdown_manager_->MarkAsShutdown(__func__, std::move(r).status());
    Shutdown();
    return;
  }
  lk.unlock();
  bool scheduled = shutdown_manager_->StartOperation(
      __func__, "OnRead", [&] { queue_.OnRead(*std::move(r)); });
  if (scheduled) {
    shutdown_manager_->FinishedOperation("OnRead");
  } else {
    std::vector<std::string> ack_ids(r->mutable_received_messages()->size());
    std::size_t total_size = 0;
    std::transform(r->mutable_received_messages()->begin(),
                   r->mutable_received_messages()->end(), ack_ids.begin(),
                   [&](google::pubsub::v1::ReceivedMessage& m) {
                     total_size += MessageProtoSize(m.message());
                     return std::move(*m.mutable_ack_id());
                   });
    (void)child_->BulkNack(std::move(ack_ids), total_size);
  }
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
