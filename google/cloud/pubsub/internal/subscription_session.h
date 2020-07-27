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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_SESSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_SESSION_H

#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/subscriber_connection.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/status_or.h"
#include <deque>
#include <memory>
#include <mutex>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class SubscriptionSession
    : public std::enable_shared_from_this<SubscriptionSession> {
 public:
  static auto constexpr kMinimumAckDeadlineSeconds = 10;
  static auto constexpr kMaximumAckDeadlineSeconds = 600;
  static auto constexpr kAckDeadlineSlackSeconds = 2;

  /// The constructor is private to prevent accidents with hared_from_this()
  static std::shared_ptr<SubscriptionSession> Create(
      std::shared_ptr<pubsub_internal::SubscriberStub> s,
      google::cloud::CompletionQueue executor,
      pubsub::SubscriberConnection::SubscribeParams p) {
    return std::shared_ptr<SubscriptionSession>(new SubscriptionSession(
        std::move(s), std::move(executor), std::move(p)));
  }

  /// Start running the subscription, pull the first batch of messages.
  future<Status> Start();

  /// Discard any information about @p ack_id and stop refreshing its deadline.
  void MessageHandled(std::string const& ack_id, std::size_t message_size);

  /// Test-only, speed up the deadline refreshes to keep tests snappy.
  void SetTestRefreshPeriod(std::chrono::milliseconds d) {
    test_refresh_period_ = d;
  }

 private:
  SubscriptionSession(std::shared_ptr<pubsub_internal::SubscriberStub> s,
                      google::cloud::CompletionQueue executor,
                      pubsub::SubscriberConnection::SubscribeParams p)
      : stub_(std::move(s)),
        executor_(std::move(executor)),
        params_(std::move(p)),
        test_refresh_period_(0) {}

  /// Stop fetching message batchines and stop updating any deadlines
  void Cancel();

  /// Asynchronously fetch one batch of messages
  void PullOne();

  /// Called when a batch of messages is received.
  void OnPull(StatusOr<google::pubsub::v1::PullResponse> r);

  void HandleQueue() { HandleQueue(std::unique_lock<std::mutex>(mu_)); }

  /// Handle the queue of messages, invoking `callback_` on the next message.
  void HandleQueue(std::unique_lock<std::mutex> lk);

  /// Start the loop to keep ack deadlines refreshed
  void StartAckDeadlineLoop() {
    RefreshAckDeadlines(std::unique_lock<std::mutex>(mu_));
  }

  /// If needed asynchronous update the ack deadlines in the server.
  void RefreshAckDeadlines(std::unique_lock<std::mutex> lk);

  /// Invoked when the asynchronous RPC to update ack deadlines completes.
  void OnRefreshAckDeadlines(
      google::pubsub::v1::ModifyAckDeadlineRequest const& request,
      std::chrono::system_clock::time_point new_server_deadline);

  /// Start the timer to update ack deadlines.
  void StartRefreshAckDeadlinesTimer(
      std::unique_lock<std::mutex> lk,
      std::chrono::system_clock::time_point new_server_deadline);

  /// The timer to update ack deadlines has triggered.
  void OnRefreshAckDeadlinesTimer();

  std::shared_ptr<pubsub_internal::SubscriberStub> stub_;
  google::cloud::CompletionQueue executor_;
  pubsub::SubscriberConnection::SubscribeParams params_;
  std::mutex mu_;
  bool shutdown_ = false;
  std::deque<google::pubsub::v1::ReceivedMessage> messages_;
  struct AckStatus {
    std::chrono::system_clock::time_point estimated_server_deadline;
    std::chrono::system_clock::time_point handling_deadline;
  };
  std::map<std::string, AckStatus> ack_deadlines_;
  promise<Status> result_;
  bool refreshing_deadlines_ = false;
  future<void> refresh_timer_;

  std::chrono::milliseconds test_refresh_period_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_SESSION_H
