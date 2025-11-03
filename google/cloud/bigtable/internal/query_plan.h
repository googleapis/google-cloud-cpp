
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
#include "google/cloud/internal/clock.h"
#include <google/bigtable/v2/bigtable.pb.h>
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
      CompletionQueue cq, google::bigtable::v2::PrepareQueryResponse response,
      RefreshFn fn, std::shared_ptr<Clock> clock = std::make_shared<Clock>());

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
            google::bigtable::v2::PrepareQueryResponse response)
      : cq_(std::move(cq)),
        clock_(std::move(clock)),
        refresh_fn_(std::move(fn)),
        response_(std::move(response)) {}

  bool IsRefreshing(std::unique_lock<std::mutex> const&) const;

  // Performs the first call to ScheduleRefresh and any other initialization not
  // possible in the constructor.
  void Initialize();

  // Calls MakeDeadlineTimer on the CompletionQueue with a continuation lambda
  // capturing a std::weak_ptr to this that calls RefreshQueryPlan.
  void ScheduleRefresh(std::unique_lock<std::mutex> const&);

  enum class RefreshMode { kExpired, kInvalidated, kAlreadyRefreshing };
  // Performs the synchronization around calling RefreshFn and updating
  // response_.
  //  void RefreshQueryPlan();

  void RefreshQueryPlan(RefreshMode mode, Status error = {});

  void ExpiredRefresh();

  // State machine where the only valid transitions are:
  //   kDone -> kBegin
  //   kBegin -> kPending
  //   kPending -> kBegin
  //   kPending -> kDone
  // When refreshing the same previous query plan.
  enum class RefreshState {
    kBegin,    // waiting for a future thread to refresh response_
    kPending,  // waiting for an active thread to refresh response_
    kDone,     // response_ has been refreshed
  };
  RefreshState state_ = RefreshState::kDone;

  CompletionQueue cq_;
  std::shared_ptr<Clock> clock_;
  RefreshFn refresh_fn_;
  future<void> refresh_timer_;
  mutable std::mutex mu_;
  std::condition_variable cond_;
  // waiting_threads_ is only a snapshot, but it helps us reduce the number of
  // RPCs in flight to refresh the same query plan.
  int waiting_threads_ = 0;
  std::string old_query_plan_id_;
  StatusOr<google::bigtable::v2::PrepareQueryResponse>
      response_;  // GUARDED_BY(mu_)
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_QUERY_PLAN_H
