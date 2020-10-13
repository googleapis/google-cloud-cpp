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

#include "google/cloud/pubsub/internal/subscription_lease_management.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

std::chrono::seconds constexpr SubscriptionLeaseManagement::kMinimumAckDeadline;
std::chrono::seconds constexpr SubscriptionLeaseManagement::kMaximumAckDeadline;
std::chrono::seconds constexpr SubscriptionLeaseManagement::kAckDeadlineSlack;

void SubscriptionLeaseManagement::Start(BatchCallback cb) {
  auto weak = std::weak_ptr<SubscriptionLeaseManagement>(shared_from_this());
  child_->Start(
      [weak, cb](StatusOr<google::pubsub::v1::StreamingPullResponse> r) {
        if (auto self = weak.lock()) self->OnRead(r);
        cb(std::move(r));
      });
}

void SubscriptionLeaseManagement::Shutdown() {
  std::unique_lock<std::mutex> lk(mu_);
  // Cancel any existing timers.
  if (refresh_timer_.valid()) refresh_timer_.cancel();
  // Schedule a nack for each pending message, using the existing executor.
  NackAll(std::move(lk));
  child_->Shutdown();
}

void SubscriptionLeaseManagement::AckMessage(std::string const& ack_id) {
  std::unique_lock<std::mutex> lk(mu_);
  leases_.erase(ack_id);
  lk.unlock();
  child_->AckMessage(ack_id);
}

void SubscriptionLeaseManagement::NackMessage(std::string const& ack_id) {
  std::unique_lock<std::mutex> lk(mu_);
  leases_.erase(ack_id);
  lk.unlock();
  child_->NackMessage(ack_id);
}

void SubscriptionLeaseManagement::BulkNack(std::vector<std::string> ack_ids) {
  std::unique_lock<std::mutex> lk(mu_);
  for (auto const& id : ack_ids) leases_.erase(id);
  lk.unlock();
  child_->BulkNack(std::move(ack_ids));
}

// Users of this class should have no need to call ExtendLeases(); they create
// it to automate lease management after all. We could create a hierarchy of
// classes for "BatchSourceWithoutExtendLeases", but that seems like overkill.
void SubscriptionLeaseManagement::ExtendLeases(std::vector<std::string>,
                                               std::chrono::seconds) {}

void SubscriptionLeaseManagement::OnRead(
    StatusOr<google::pubsub::v1::StreamingPullResponse> const& response) {
  if (!response) {
    shutdown_manager_->MarkAsShutdown(__func__, response.status());
    std::unique_lock<std::mutex> lk(mu_);
    // Cancel any existing timers.
    if (refresh_timer_.valid()) refresh_timer_.cancel();
    return;
  }
  std::unique_lock<std::mutex> lk(mu_);
  auto const now = std::chrono::system_clock::now();
  auto const estimated_server_deadline = now + std::chrono::seconds(10);
  auto const handling_deadline = now + max_deadline_time_;
  for (auto const& rm : response->received_messages()) {
    leases_.emplace(rm.ack_id(),
                    LeaseStatus{estimated_server_deadline, handling_deadline});
  }
  // Setup a timer to refresh the message leases. We do not want to immediately
  // refresh them because there is a good chance they will be handled before
  // the minimum lease time, and it seems wasteful to refresh the lease just to
  // quickly turnaround and ack or nack the message.
  StartRefreshTimer(std::move(lk),
                    std::chrono::system_clock::now() + kMinimumAckDeadline);
}

void SubscriptionLeaseManagement::RefreshMessageLeases(
    std::unique_lock<std::mutex> lk) {
  using seconds = std::chrono::seconds;

  if (leases_.empty() || refreshing_leases_) return;

  std::vector<std::string> ack_ids;
  ack_ids.reserve(leases_.size());
  auto extension = kMaximumAckDeadline;
  auto const now = std::chrono::system_clock::now();
  for (auto const& kv : leases_) {
    // This message lease cannot be extended any further, and we do not want to
    // send an extension of 0 seconds because that is a nack.
    if (kv.second.handling_deadline < now + seconds(1)) continue;
    auto const message_extension =
        std::chrono::duration_cast<seconds>(kv.second.handling_deadline - now);
    extension = (std::min)(extension, message_extension);
    ack_ids.push_back(kv.first);
  }
  auto const new_deadline = now + extension;
  if (ack_ids.empty()) {
    StartRefreshTimer(std::move(lk), new_deadline);
    return;
  }
  lk.unlock();
  child_->ExtendLeases(ack_ids, extension);
  lk.lock();
  for (auto const& ack : ack_ids) {
    auto i = leases_.find(ack);
    if (i == leases_.end()) continue;
    i->second.estimated_server_deadline = new_deadline;
  }
  StartRefreshTimer(std::move(lk), new_deadline);
}

void SubscriptionLeaseManagement::StartRefreshTimer(
    std::unique_lock<std::mutex>,
    std::chrono::system_clock::time_point new_server_deadline) {
  std::weak_ptr<SubscriptionLeaseManagement> weak = shared_from_this();
  auto deadline = new_server_deadline - kAckDeadlineSlack;

  shutdown_manager_->StartOperation(__func__, "OnRefreshTimer", [&] {
    if (refresh_timer_.valid()) refresh_timer_.cancel();
    refresh_timer_ = timer_factory_(deadline).then([weak](future<Status> f) {
      if (auto self = weak.lock()) self->OnRefreshTimer(!f.get().ok());
    });
  });
}

void SubscriptionLeaseManagement::OnRefreshTimer(bool cancelled) {
  if (shutdown_manager_->FinishedOperation(__func__)) return;
  if (cancelled) return;
  RefreshMessageLeases(std::unique_lock<std::mutex>(mu_));
}

void SubscriptionLeaseManagement::NackAll(std::unique_lock<std::mutex> lk) {
  if (leases_.empty()) return;
  std::vector<std::string> ack_ids;
  ack_ids.reserve(leases_.size());
  for (auto const& kv : leases_) {
    ack_ids.push_back(kv.first);
  }
  lk.unlock();
  BulkNack(std::move(ack_ids));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
