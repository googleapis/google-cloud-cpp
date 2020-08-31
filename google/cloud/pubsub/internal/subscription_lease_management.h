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
#include <chrono>
#include <memory>

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
  static auto constexpr kMaximumAckDeadline = std::chrono::seconds(600);

  /**
   * A wrapper to create timers.
   *
   * TODO(#4718) - this is only needed because we cannot mock CompletionQueue.
   */
  using TimerFactory =
      std::function<future<Status>(std::chrono::system_clock::time_point)>;

  static std::shared_ptr<SubscriptionLeaseManagement> Create(
      google::cloud::CompletionQueue cq,
      std::shared_ptr<SessionShutdownManager> shutdown_manager,
      std::shared_ptr<SubscriberStub> stub, std::string subscription_full_name,
      std::chrono::seconds max_deadline_time) {
    auto timer_factory =
        [cq](std::chrono::system_clock::time_point tp) mutable {
          return cq.MakeDeadlineTimer(tp).then(
              [](future<StatusOr<std::chrono::system_clock::time_point>> f) {
                return f.get().status();
              });
        };
    return std::shared_ptr<SubscriptionLeaseManagement>(
        new SubscriptionLeaseManagement(
            std::move(cq), std::move(shutdown_manager),
            std::move(timer_factory), std::move(stub),
            std::move(subscription_full_name), max_deadline_time));
  }

  static std::shared_ptr<SubscriptionLeaseManagement> CreateForTesting(
      google::cloud::CompletionQueue cq,
      std::shared_ptr<SessionShutdownManager> shutdown_manager,
      TimerFactory timer_factory, std::shared_ptr<SubscriberStub> stub,
      std::string subscription_full_name,
      std::chrono::seconds max_deadline_time) {
    return std::shared_ptr<SubscriptionLeaseManagement>(
        new SubscriptionLeaseManagement(
            std::move(cq), std::move(shutdown_manager),
            std::move(timer_factory), std::move(stub),
            std::move(subscription_full_name), max_deadline_time));
  }

  void Shutdown() override;
  future<Status> AckMessage(std::string const& ack_id,
                            std::size_t size) override;
  future<Status> NackMessage(std::string const& ack_id,
                             std::size_t size) override;
  future<Status> BulkNack(std::vector<std::string> ack_ids,
                          std::size_t total_size) override;
  future<StatusOr<google::pubsub::v1::PullResponse>> Pull(
      std::int32_t max_count) override;

 private:
  SubscriptionLeaseManagement(
      google::cloud::CompletionQueue cq,
      std::shared_ptr<SessionShutdownManager> shutdown_manager,
      TimerFactory timer_factory, std::shared_ptr<SubscriberStub> stub,
      std::string subscription_full_name,
      std::chrono::seconds max_deadline_time)
      : cq_(std::move(cq)),
        timer_factory_(std::move(timer_factory)),
        stub_(std::move(stub)),
        shutdown_manager_(std::move(shutdown_manager)),
        subscription_full_name_(std::move(subscription_full_name)),
        max_deadline_time_(max_deadline_time) {}

  void OnPull(StatusOr<google::pubsub::v1::PullResponse> const& response);

  /// If needed asynchronous update the message leases in the server.
  void RefreshMessageLeases(std::unique_lock<std::mutex> lk);

  /// Invoked when the asynchronous RPC to update message leases completes.
  void OnRefreshMessageLeases(
      google::pubsub::v1::ModifyAckDeadlineRequest const& request,
      std::chrono::system_clock::time_point new_server_deadline);

  /// Start the timer to update ack deadlines.
  void StartRefreshTimer(
      std::unique_lock<std::mutex> lk,
      std::chrono::system_clock::time_point new_server_deadline);

  /// The timer to update ack deadlines has triggered or was cancelled.
  void OnRefreshTimer(bool cancelled);

  void NackAll(std::unique_lock<std::mutex> lk);

  google::cloud::CompletionQueue cq_;
  TimerFactory const timer_factory_;
  std::shared_ptr<SubscriberStub> const stub_;
  std::shared_ptr<SessionShutdownManager> const shutdown_manager_;
  std::string const subscription_full_name_;
  std::chrono::seconds const max_deadline_time_;

  std::mutex mu_;

  // A collection of message ack_ids to maintain the message leases.
  struct LeaseStatus {
    std::chrono::system_clock::time_point estimated_server_deadline;
    std::chrono::system_clock::time_point handling_deadline;
  };
  std::map<std::string, LeaseStatus> leases_;

  bool refreshing_leases_ = false;
  future<void> refresh_timer_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_LEASE_MANAGEMENT_H
