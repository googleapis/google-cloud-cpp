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
auto constexpr kRefreshDeadlineOffsetMs = 1000;
}  // namespace

std::shared_ptr<QueryPlan> QueryPlan::Create(
    CompletionQueue cq, google::bigtable::v2::PrepareQueryResponse response,
    RefreshFn fn, std::shared_ptr<Clock> clock) {
  auto plan = std::shared_ptr<QueryPlan>(new QueryPlan(
      std::move(cq), std::move(clock), std::move(fn), std::move(response)));
  plan->Initialize();
  return plan;
}

void QueryPlan::Initialize() {
  std::unique_lock<std::mutex> lock(mu_);
  ScheduleRefresh(lock);
}

// ScheduleRefresh should only be called after updating response_.
void QueryPlan::ScheduleRefresh(std::unique_lock<std::mutex> const&) {
  if (!response_.ok()) return;
  // We want to start the refresh process before the query plan expires.
  auto refresh_deadline =
      internal::ToChronoTimePoint(response_->valid_until()) -
      std::chrono::milliseconds(kRefreshDeadlineOffsetMs);
  std::weak_ptr<QueryPlan> plan = shared_from_this();
  refresh_timer_ =
      cq_.MakeDeadlineTimer(refresh_deadline)
          .then([plan](future<StatusOr<std::chrono::system_clock::time_point>>
                           result) {
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
  RefreshQueryPlan(RefreshMode::kExpired);
}

void QueryPlan::Invalidate(Status status,
                           std::string const& invalid_query_plan_id) {
  {
    std::unique_lock<std::mutex> lock(mu_);
    // We want to avoid a late arrival causing a refresh of an already refreshed
    // query plan, so we track what the previous plan id was.
    if (!IsRefreshing(lock) && old_query_plan_id_ != invalid_query_plan_id) {
      old_query_plan_id_ = invalid_query_plan_id;
      state_ = RefreshState::kBegin;
    }
  }
  RefreshQueryPlan(RefreshMode::kInvalidated, std::move(status));
}

void QueryPlan::RefreshQueryPlan(RefreshMode mode, Status error) {
  {
    std::unique_lock<std::mutex> lock_1(mu_);
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_QUERY_PLAN_REFRESH_ASSERT
    assert(waiting_threads_ >= 0);
#endif
    ++waiting_threads_;
    cond_.wait(lock_1, [this] { return state_ != RefreshState::kPending; });
    --waiting_threads_;
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_QUERY_PLAN_REFRESH_ASSERT
    assert(waiting_threads_ >= 0);
#endif
    if (state_ == RefreshState::kDone) return;
    if (mode == RefreshMode::kInvalidated) response_ = std::move(error);
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
      refresh_timer_.cancel();
      ScheduleRefresh(lock_2);
    } else {
      // If there are no waiting threads that could call the refresh_fn, then
      // we need to accept that the refresh is in a failed state and wait for
      // some new event that would start this refresh process anew.
      //
      // If there are waiting threads, then we want to try again to get a
      // refreshed query plan, but we want to avoid a stampede of refresh RPCs
      // so we only notify one of the waiting threads.
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_QUERY_PLAN_REFRESH_ASSERT
      assert(waiting_threads_ >= 0);
#endif
      if (waiting_threads_ == 0) {
        state_ = RefreshState::kDone;
        done = true;
      }
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
    RefreshQueryPlan(RefreshMode::kAlreadyRefreshing);
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
