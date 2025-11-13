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

#include "google/cloud/bigtable/internal/query_plan.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/internal/time_utils.h"
#include <google/bigtable/v2/data.pb.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
auto constexpr kRefreshDeadlineOffsetMs = std::chrono::milliseconds(1000);
}  // namespace

std::shared_ptr<QueryPlan> QueryPlan::Create(
    CompletionQueue cq,
    StatusOr<google::bigtable::v2::PrepareQueryResponse> response, RefreshFn fn,
    std::shared_ptr<Clock> clock) {
  auto plan = std::shared_ptr<QueryPlan>(new QueryPlan(
      std::move(cq), std::move(clock), std::move(fn), std::move(response)));
  plan->Initialize();
  return plan;
}

QueryPlan::~QueryPlan() {
  if (refresh_timer_.valid()) refresh_timer_.cancel();
}

void QueryPlan::Initialize() {
  std::unique_lock<std::mutex> lock(mu_);
  if (state_ == RefreshState::kDone) ScheduleRefresh(lock);
}

// ScheduleRefresh should only be called after updating response_.
void QueryPlan::ScheduleRefresh(std::unique_lock<std::mutex> const&) {
  if (!response_.ok()) return;
  // We want to start the refresh process before the query plan expires.
  auto refresh_deadline =
      internal::ToChronoTimePoint(response_->valid_until()) -
      kRefreshDeadlineOffsetMs;
  std::weak_ptr<QueryPlan> plan = shared_from_this();
  refresh_timer_ =
      cq_.MakeDeadlineTimer(refresh_deadline)
          .then([plan, current = internal::SaveCurrentOptions()](
                    future<StatusOr<std::chrono::system_clock::time_point>>
                        result) {
            // Options are stored in a thread_local variable. When this timer
            // expires and this lambda is executed we need to restore the
            // Options that were saved in the capture group as a different
            // thread may be used.
            internal::OptionsSpan options_span(current);
            if (result.get().ok()) {
              if (auto p = plan.lock()) {
                p->ExpiredRefresh();
              }
            }
          });
}

bool QueryPlan::IsRefreshing(std::unique_lock<std::mutex> const&) const {
  return state_ == RefreshState::kBegin || state_ == RefreshState::kPending;
}

void QueryPlan::ExpiredRefresh() {
  {
    std::unique_lock<std::mutex> lock(mu_);
    if (!(IsRefreshing(lock))) {
      if (response_.ok()) old_query_plan_id_ = response_->prepared_query();
      state_ = RefreshState::kBegin;
    }
  }
  RefreshQueryPlan();
}

void QueryPlan::Invalidate(Status status,
                           std::string const& invalid_query_plan_id) {
  {
    std::unique_lock<std::mutex> lock(mu_);
    // We want to avoid a late arrival causing a refresh of an already refreshed
    // query plan, so we track what the previous plan id was.
    if (!IsRefreshing(lock) && old_query_plan_id_ != invalid_query_plan_id) {
      old_query_plan_id_ = invalid_query_plan_id;
      response_ = std::move(status);
      state_ = RefreshState::kBegin;
    }
  }
}

void QueryPlan::RefreshQueryPlan() {
  {
    std::unique_lock<std::mutex> lock_1(mu_);
    cond_.wait(lock_1, [this] { return state_ != RefreshState::kPending; });
    if (state_ == RefreshState::kDone) return;
    state_ = RefreshState::kPending;
  }
  auto response = refresh_fn_().get();
  bool done = false;
  {
    std::unique_lock<std::mutex> lock_2(mu_);
    response_ = std::move(response);
    if (response_.ok()) {
      state_ = RefreshState::kDone;
      done = true;
      // If we have to refresh an invalidated query plan, cancel any existing
      // timer before starting a new one.
      if (refresh_timer_.valid()) refresh_timer_.cancel();
      ScheduleRefresh(lock_2);
    } else {
      // If there are no waiting threads that could call the refresh_fn, then
      // we need to accept that the refresh is in a failed state and wait for
      // some new event that would start this refresh process anew.
      //
      // If there are waiting threads, then we want to try again to get a
      // refreshed query plan, but we want to avoid a stampede of refresh RPCs
      // so we only notify one of the waiting threads.
      state_ = RefreshState::kBegin;
    }
  }
  if (done) {
    cond_.notify_all();
  } else {
    cond_.notify_one();
  }
}

StatusOr<google::bigtable::v2::PrepareQueryResponse> QueryPlan::response() {
  std::unique_lock<std::mutex> lock(mu_);
  if (IsRefreshing(lock)) {
    if (response_.ok()) {
      return response_;
    }
    lock.unlock();
    RefreshQueryPlan();
    lock.lock();
  }

  if (state_ == RefreshState::kDone && !response_.ok()) {
    return response_.status();
  }

  return response_;
}

StatusOr<std::string> QueryPlan::prepared_query() {
  auto data = response();
  if (!data.ok()) return data.status();
  return response_->prepared_query();
}

StatusOr<google::bigtable::v2::ResultSetMetadata> QueryPlan::metadata() {
  auto data = response();
  if (!data.ok()) return data.status();
  return response_->metadata();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
