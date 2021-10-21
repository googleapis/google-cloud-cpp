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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_LEASE_MANAGEMENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_LEASE_MANAGEMENT_H

#include "google/cloud/pubsub/internal/session_shutdown_manager.h"
#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/internal/subscription_batch_source.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/absl_flat_hash_map_quiet.h"
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class SubscriptionLeaseManagement
    : public SubscriptionBatchSource,
      public std::enable_shared_from_this<SubscriptionLeaseManagement> {
 public:
  static auto constexpr kAckDeadlineSlack = std::chrono::seconds(2);
  static auto constexpr kMinimumAckDeadline = std::chrono::seconds(10);

  static std::shared_ptr<SubscriptionLeaseManagement> Create(
      google::cloud::CompletionQueue cq,
      std::shared_ptr<SessionShutdownManager> shutdown_manager,
      std::shared_ptr<SubscriptionBatchSource> child,
      std::chrono::seconds max_deadline_time,
      std::chrono::seconds max_deadline_extension) {
    return std::shared_ptr<SubscriptionLeaseManagement>(
        new SubscriptionLeaseManagement(
            std::move(cq), std::move(shutdown_manager), std::move(child),
            max_deadline_time, max_deadline_extension));
  }

  void Start(BatchCallback cb) override;
  void Shutdown() override;
  void AckMessage(std::string const& ack_id) override;
  void NackMessage(std::string const& ack_id) override;
  void BulkNack(std::vector<std::string> ack_ids) override;
  void ExtendLeases(std::vector<std::string> ack_ids,
                    std::chrono::seconds extension) override;

 private:
  SubscriptionLeaseManagement(
      google::cloud::CompletionQueue cq,
      std::shared_ptr<SessionShutdownManager> shutdown_manager,
      std::shared_ptr<SubscriptionBatchSource> child,
      std::chrono::seconds max_deadline_time,
      std::chrono::seconds max_deadline_extension)
      : cq_(std::move(cq)),
        child_(std::move(child)),
        shutdown_manager_(std::move(shutdown_manager)),
        max_deadline_time_(max_deadline_time),
        max_deadline_extension_(max_deadline_extension) {}

  void OnRead(
      StatusOr<google::pubsub::v1::StreamingPullResponse> const& response);

  /// If needed asynchronous update the message leases in the server.
  void RefreshMessageLeases(std::unique_lock<std::mutex> lk);

  /// Start the timer to update ack deadlines.
  void StartRefreshTimer(
      std::unique_lock<std::mutex> lk,
      std::chrono::system_clock::time_point new_server_deadline);

  /// The timer to update ack deadlines has triggered or was cancelled.
  void OnRefreshTimer(bool cancelled);

  void NackAll(std::unique_lock<std::mutex> lk);

  google::cloud::CompletionQueue cq_;
  std::shared_ptr<SubscriptionBatchSource> const child_;
  std::shared_ptr<SessionShutdownManager> const shutdown_manager_;
  std::chrono::seconds const max_deadline_time_;
  std::chrono::seconds const max_deadline_extension_;

  std::mutex mu_;

  // A collection of message ack_ids to maintain the message leases.
  struct LeaseStatus {
    std::chrono::system_clock::time_point estimated_server_deadline;
    std::chrono::system_clock::time_point handling_deadline;
  };
  absl::flat_hash_map<std::string, LeaseStatus> leases_;

  bool refreshing_leases_ = false;
  future<void> refresh_timer_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_LEASE_MANAGEMENT_H
