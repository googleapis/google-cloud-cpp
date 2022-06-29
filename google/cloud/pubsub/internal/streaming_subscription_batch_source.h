// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
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
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class StreamingSubscriptionBatchSource
    : public SubscriptionBatchSource,
      public std::enable_shared_from_this<StreamingSubscriptionBatchSource> {
 public:
  explicit StreamingSubscriptionBatchSource(
      CompletionQueue cq,
      std::shared_ptr<SessionShutdownManager> shutdown_manager,
      std::shared_ptr<SubscriberStub> stub, std::string subscription_full_name,
      std::string client_id, Options opts);

  ~StreamingSubscriptionBatchSource() override = default;

  void Start(BatchCallback callback) override;

  void Shutdown() override;
  future<Status> AckMessage(std::string const& ack_id) override;
  future<Status> NackMessage(std::string const& ack_id) override;
  future<Status> BulkNack(std::vector<std::string> ack_ids) override;
  void ExtendLeases(std::vector<std::string> ack_ids,
                    std::chrono::seconds extension) override;

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

  void UpdateStreamDeadline();
  void OnWrite(bool ok);

  void ChangeState(std::unique_lock<std::mutex> const& lk, StreamState s,
                   char const* where, char const* reason);

  CompletionQueue cq_;
  std::shared_ptr<SessionShutdownManager> const shutdown_manager_;
  std::shared_ptr<SubscriberStub> const stub_;
  std::string const subscription_full_name_;
  std::string const client_id_;
  Options options_;
  std::int64_t const max_outstanding_messages_;
  std::int64_t const max_outstanding_bytes_;
  std::chrono::seconds const min_deadline_time_;
  std::chrono::seconds const max_deadline_time_;

  std::mutex mu_;
  BatchCallback callback_;
  StreamState stream_state_ = StreamState::kNull;
  bool shutdown_ = false;
  bool pending_write_ = false;
  bool pending_read_ = false;
  Status status_;
  std::shared_ptr<AsyncPullStream> stream_;
  absl::optional<bool> exactly_once_delivery_enabled_;
  std::vector<std::pair<std::string, std::chrono::seconds>> deadlines_queue_;
};

std::ostream& operator<<(std::ostream& os,
                         StreamingSubscriptionBatchSource::StreamState s);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_STREAMING_SUBSCRIPTION_BATCH_SOURCE_H
