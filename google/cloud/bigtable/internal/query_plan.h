// Copyright 2025 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_QUERY_PLAN_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_QUERY_PLAN_H

#include "google/cloud/bigtable/version.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/clock.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/bigtable/v2/bigtable.pb.h"
#include "google/bigtable/v2/data.pb.h"
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class QueryPlan : public std::enable_shared_from_this<QueryPlan> {
 public:
  // Typically, a lambda capturing the original PrepareQueryRequest and
  // DataConnection pointer necessary to call the AsyncPrepareQuery RPC.
  using RefreshFn = std::function<
      future<StatusOr<google::bigtable::v2::PrepareQueryResponse>>()>;

  using Clock = ::google::cloud::internal::SystemClock;

  // Calls the constructor and then Initialize.
  static std::shared_ptr<QueryPlan> Create(
      CompletionQueue cq,
      StatusOr<google::bigtable::v2::PrepareQueryResponse> response,
      RefreshFn fn, std::shared_ptr<Clock> clock = std::make_shared<Clock>());

  ~QueryPlan();

  // Invalidates the current QueryPlan and triggers a refresh.
  void Invalidate(Status status, std::string const& invalid_query_plan_id);

  // Accessor for the prepared_query and metadata fields in response_.
  // Triggers a refresh if needed.
  StatusOr<google::bigtable::v2::PrepareQueryResponse> response();

  GOOGLE_CLOUD_CPP_DEPRECATED("Use response() instead")
  StatusOr<std::string> prepared_query();

  GOOGLE_CLOUD_CPP_DEPRECATED("Use response() instead")
  StatusOr<google::bigtable::v2::ResultSetMetadata> metadata();

 private:
  QueryPlan(CompletionQueue cq, std::shared_ptr<Clock> clock, RefreshFn fn,
            StatusOr<google::bigtable::v2::PrepareQueryResponse> response)
      : state_(response.ok() ? RefreshState::kDone : RefreshState::kBegin),
        cq_(std::move(cq)),
        clock_(std::move(clock)),
        refresh_fn_(std::move(fn)),
        response_(std::move(response)) {}

  // Performs the first call to ScheduleRefresh and any other initialization not
  // possible in the constructor.
  void Initialize();

  // Calls MakeDeadlineTimer on the CompletionQueue with a continuation lambda
  // capturing a std::weak_ptr to this that triggers StartRefresh
  // asynchronously. Temporarily unlocks `lock` while cancelling any prior
  // timer and scheduling the new timer.
  void ScheduleRefresh(std::unique_lock<std::mutex>& lock);

  // Starts an asynchronous refresh via RefreshFn without blocking the calling
  // thread (which may be a single-threaded CompletionQueue worker).
  // Temporarily unlocks `lock` while invoking `refresh_fn_`.
  void StartRefresh(std::unique_lock<std::mutex>& lock);

  // Updates `response_` and `state_` when an asynchronous refresh completes.
  void OnRefreshCompleted(
      StatusOr<google::bigtable::v2::PrepareQueryResponse> response);

  // State machine transitions:
  //   kDone -> kBegin (on Invalidate when no refresh is in flight)
  //   kDone -> kPending (proactive timer refresh started)
  //   kBegin -> kPending (reactive refresh started)
  //   kPending -> kBegin (refresh failed)
  //   kPending -> kDone (refresh succeeded)
  enum class RefreshState {
    kBegin,    // waiting for a future thread to start refreshing response_
    kPending,  // asynchronous refresh_fn_() call is in flight
    kDone,     // response_ holds a valid query plan with no refresh in flight
  };
  RefreshState state_;  // GUARDED_BY(mu_)

  CompletionQueue cq_;
  std::shared_ptr<Clock> clock_;
  RefreshFn refresh_fn_;
  mutable std::mutex mu_;
  std::condition_variable cond_;
  future<void> refresh_timer_;            // GUARDED_BY(mu_)
  future<void> refresh_op_;               // GUARDED_BY(mu_)
  std::uint64_t timer_generation_ = 0;    // GUARDED_BY(mu_)
  std::uint64_t op_generation_ = 0;       // GUARDED_BY(mu_)
  std::uint64_t refresh_generation_ = 0;  // GUARDED_BY(mu_)
  StatusOr<google::bigtable::v2::PrepareQueryResponse>
      response_;  // GUARDED_BY(mu_)
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_QUERY_PLAN_H
