// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_CONNECTION_REFRESH_STATE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_CONNECTION_REFRESH_STATE_H

#include "google/cloud/bigtable/version.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/random.h"
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class OutstandingTimers
    : public std::enable_shared_from_this<OutstandingTimers> {
 public:
  explicit OutstandingTimers(std::shared_ptr<CompletionQueue> const& cq)
      : weak_cq_(cq) {}
  // Register a timer. It will automatically deregister on completion.
  void RegisterTimer(future<void> fut);
  // Cancel all currently registered timers and all which will be registered in
  // the future.
  void CancelAll();

 private:
  void DeregisterTimer(std::uint64_t id);
  std::mutex mu_;
  bool shutdown_ = false;           // GUARDED_BY(mu_)
  std::uint64_t id_generator_ = 0;  // GUARDED_BY(mu_)
  std::unordered_map<std::uint64_t,
                     future<void>> timers_;  // GUARDED_BY(mu_)
  // Object of this class is owned by timers continuations, which means it
  // cannot have an owning reference to the `CompletionQueue` because  it would
  // otherwise create a risk of a deadlock on the completion queue destruction.
  std::weak_ptr<CompletionQueue> weak_cq_;  // GUARDED_BY(mu_)
};

/**
 * State required by timers scheduled by `CommonClient`.
 *
 * The scheduled timers might outlive `CommonClient`. They need some shared,
 * persistent state. Objects of this class implement it.
 */
class ConnectionRefreshState {
 public:
  explicit ConnectionRefreshState(
      std::shared_ptr<CompletionQueue> const& cq,
      std::chrono::milliseconds min_conn_refresh_period,
      std::chrono::milliseconds max_conn_refresh_period);
  std::chrono::milliseconds RandomizedRefreshDelay();
  OutstandingTimers& timers() { return *timers_; }
  bool enabled() const;

 private:
  std::mutex mu_;
  std::chrono::milliseconds min_conn_refresh_period_;
  std::chrono::milliseconds max_conn_refresh_period_;
  google::cloud::internal::DefaultPRNG rng_;
  std::shared_ptr<OutstandingTimers> timers_;
};

/**
 * Schedule a chain of timers to refresh the connection.
 */
void ScheduleChannelRefresh(
    std::shared_ptr<CompletionQueue> const& cq,
    std::shared_ptr<ConnectionRefreshState> const& state,
    std::shared_ptr<grpc::Channel> const& channel);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_CONNECTION_REFRESH_STATE_H
