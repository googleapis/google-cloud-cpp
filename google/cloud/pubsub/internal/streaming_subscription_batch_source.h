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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_STREAMING_SUBSCRIPTION_BATCH_SOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_STREAMING_SUBSCRIPTION_BATCH_SOURCE_H

#include "google/cloud/pubsub/backoff_policy.h"
#include "google/cloud/pubsub/internal/session_shutdown_manager.h"
#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/internal/subscription_batch_source.h"
#include "google/cloud/pubsub/retry_policy.h"
#include "google/cloud/pubsub/subscriber_options.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/absl_flat_hash_map_quiet.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <google/pubsub/v1/pubsub.pb.h>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * Configuration parameters to batch Ack/Nack responses.
 *
 * To minimize I/O overhead we batch the Ack/Nack responses from the application
 * into larger `Write()` requests. Some test set these numbers to different
 * values, and in the future we may expose them to the application via
 * `pubsub::SubscriberOptions`. For now they are only available in
 * `pubsub_internal` because it is always easy to add new APIs later vs.
 * removing these any APIs or accessors.
 */
struct AckBatchingConfig {
  AckBatchingConfig() = default;
  AckBatchingConfig(std::size_t s, std::chrono::milliseconds t)
      : max_batch_size(s), max_hold_time(t) {}

  // The defaults are biased towards high-throughput applications. Note that
  // the max_hold_time is small enough that it should not make a big difference,
  // the minimum ack deadline is 10 seconds.
  std::size_t max_batch_size = 1000;
  std::chrono::milliseconds max_hold_time{100};
};

/**
 * Keep a single StreamingPull active.
 *
 * Creates a single StreamingPull request active:
 *
 * - Once successfully connected [1], it resumes the request if the streaming
 *   pull is terminated with any failure.
 * - Sends at most one `Read()` and at most one `Write()` request to the
 *   underlying gRPC abstraction.
 * - On shutdown, wait for any pending `Read()` and `Write()` requests to
 *   complete before calling `Finish()`.
 * - Invoke a callback when `Read()` receives data.
 * - To preserve ordering, only once the callback returns is a new `Read()`
 *   request issued.
 * - To minimize I/O buffers nacks and acks into a single `Write()` request.
 * - Periodically flush any buffered acks or nacks.
 * - Periodically update the leases for any messages that have not been acked
 *   nor nacked.
 *
 * For testing purposes the clock may be mocked via a `std::function<>`.
 *
 * Flushing and lease updates are implemented using a simple periodic timer, the
 * timer remains active even when there is no data to flush.
 *
 * [1]: A streaming pull is "connected" once the first `Read()` request
 *      completes without error.
 */
class StreamingSubscriptionBatchSource
    : public SubscriptionBatchSource,
      public std::enable_shared_from_this<StreamingSubscriptionBatchSource> {
 public:
  using Clock = std::function<std::chrono::system_clock::time_point(void)>;

  static auto constexpr kMinimumAckDeadline = std::chrono::seconds(10);
  static auto constexpr kMaximumAckDeadline = std::chrono::seconds(600);

  struct TimerConfig {
    TimerConfig() : clock([] { return std::chrono::system_clock::now(); }) {}

    Clock clock;
    std::chrono::milliseconds period{10};
    std::chrono::milliseconds lease_refresh{kMinimumAckDeadline / 2};
  };

  explicit StreamingSubscriptionBatchSource(
      google::cloud::CompletionQueue cq,
      std::shared_ptr<SessionShutdownManager> shutdown_manager,
      std::shared_ptr<SubscriberStub> stub, std::string subscription_full_name,
      std::string client_id, pubsub::SubscriberOptions const& options,
      std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
      std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy,
      AckBatchingConfig ack_batching_config = {}, TimerConfig timer_config = {})
      : cq_(std::move(cq)),
        shutdown_manager_(std::move(shutdown_manager)),
        stub_(std::move(stub)),
        subscription_full_name_(std::move(subscription_full_name)),
        client_id_(std::move(client_id)),
        max_outstanding_messages_(options.max_outstanding_messages()),
        max_outstanding_bytes_(options.max_outstanding_bytes()),
        max_deadline_time_(options.max_deadline_time()),
        retry_policy_(std::move(retry_policy)),
        backoff_policy_(std::move(backoff_policy)),
        ack_batching_config_(std::move(ack_batching_config)),
        timer_config_(std::move(timer_config)) {}

  ~StreamingSubscriptionBatchSource() override = default;

  void Start(BatchCallback callback) override;

  void Shutdown() override;
  void AckMessage(std::string const& ack_id) override;
  void NackMessage(std::string const& ack_id) override;
  void BulkNack(std::vector<std::string> ack_ids) override;

  using AsyncPullStream = SubscriberStub::AsyncPullStream;

  enum class StreamState {
    kNull,
    kActive,
    kDisconnecting,
    kFinishing,
  };

 private:
  // C++17 adds weak_from_this(), we cannot use the same name as (1) some
  // versions of the standard library include `weak_from_this()` even with
  // just C++11 enabled, and (2) we want to support C++17 builds.
  std::weak_ptr<StreamingSubscriptionBatchSource> WeakFromThis() {
    return {shared_from_this()};
  }

  void StartStream(std::shared_ptr<pubsub::RetryPolicy> retry_policy,
                   std::shared_ptr<pubsub::BackoffPolicy> backoff_policy);

  struct RetryLoopState {
    std::shared_ptr<pubsub::RetryPolicy> retry_policy;
    std::shared_ptr<pubsub::BackoffPolicy> backoff_policy;
  };

  google::pubsub::v1::StreamingPullRequest InitialRequest() const;
  void OnStart(RetryLoopState rs,
               google::pubsub::v1::StreamingPullRequest const& request,
               bool ok);
  void OnInitialWrite(RetryLoopState const& rs, bool ok);
  void OnInitialRead(
      RetryLoopState rs,
      absl::optional<google::pubsub::v1::StreamingPullResponse> response);
  void OnInitialError(RetryLoopState rs);
  void OnInitialFinish(RetryLoopState rs, Status status);
  void OnBackoff(RetryLoopState rs, Status status);
  void OnRetryFailure(Status status);

  void ReadLoop();

  void OnRead(
      absl::optional<google::pubsub::v1::StreamingPullResponse> response);
  void ShutdownStream(std::unique_lock<std::mutex> lk, char const* reason);
  void OnFinish(Status status);

  void DrainQueues(std::unique_lock<std::mutex> lk, bool force_flush);
  void OnWrite(bool ok);

  void StartTimer();
  void OnTimer(Status const&);

  void ChangeState(std::unique_lock<std::mutex> const& lk, StreamState s,
                   char const* where, char const* reason);

  google::cloud::CompletionQueue cq_;
  std::shared_ptr<SessionShutdownManager> const shutdown_manager_;
  std::shared_ptr<SubscriberStub> const stub_;
  std::string const subscription_full_name_;
  std::string const client_id_;
  std::int64_t const max_outstanding_messages_;
  std::int64_t const max_outstanding_bytes_;
  std::chrono::seconds const max_deadline_time_;
  std::unique_ptr<pubsub::RetryPolicy const> retry_policy_;
  std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy_;
  AckBatchingConfig const ack_batching_config_;
  TimerConfig const timer_config_;

  std::mutex mu_;
  BatchCallback callback_;
  StreamState stream_state_ = StreamState::kNull;
  bool shutdown_ = false;
  bool pending_write_ = false;
  bool pending_read_ = false;
  Status status_;
  std::shared_ptr<AsyncPullStream> stream_;
  std::vector<std::string> ack_queue_;
  std::vector<std::string> nack_queue_;

  struct LeaseStatus {
    std::chrono::system_clock::time_point maximum_extension;
  };
  absl::flat_hash_map<std::string, LeaseStatus> leases_;
  std::chrono::system_clock::time_point flush_deadline_;
  std::chrono::system_clock::time_point lease_refresh_deadline_;
};

std::ostream& operator<<(std::ostream& os,
                         StreamingSubscriptionBatchSource::StreamState s);

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_STREAMING_SUBSCRIPTION_BATCH_SOURCE_H
