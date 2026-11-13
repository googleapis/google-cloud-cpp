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
#include "google/cloud/options.h"
#include "google/bigtable/v2/data.pb.h"
#include <chrono>

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
  future<void> timer;
  future<void> op;
  {
    std::lock_guard<std::mutex> lock(mu_);
    ++timer_generation_;
    ++op_generation_;
    timer = std::move(refresh_timer_);
    op = std::move(refresh_op_);
  }
  if (timer.valid()) timer.cancel();
  if (op.valid()) op.cancel();
}

void QueryPlan::Initialize() {
  std::unique_lock<std::mutex> lock(mu_);
  if (state_ == RefreshState::kDone) ScheduleRefresh(lock);
}

// ScheduleRefresh should only be called after updating response_.
void QueryPlan::ScheduleRefresh(std::unique_lock<std::mutex>& lock) {
  if (!response_.ok()) return;
  // We want to start the refresh process before the query plan expires.
  std::chrono::system_clock::time_point const refresh_deadline =
      internal::ToChronoTimePoint(response_->valid_until()) -
      kRefreshDeadlineOffsetMs;
  future<void> prev_timer = std::move(refresh_timer_);
  std::uint64_t const timer_id = ++timer_generation_;
  std::weak_ptr<QueryPlan> plan = weak_from_this();
  internal::ImmutableOptions current = internal::SaveCurrentOptions();
  lock.unlock();

  if (prev_timer.valid()) prev_timer.cancel();
  future<void> timer =
      cq_.MakeDeadlineTimer(refresh_deadline)
          .then([plan = std::move(plan), current = std::move(current)](
                    future<StatusOr<std::chrono::system_clock::time_point>>
                        result) {
            // Options are stored in a thread_local variable. When this timer
            // expires and this lambda is executed we need to restore the
            // Options that were saved in the capture group as a different
            // thread may be used.
            internal::OptionsSpan options_span(current);
            if (result.get().ok()) {
              if (std::shared_ptr<QueryPlan> p = plan.lock()) {
                std::unique_lock<std::mutex> lk(p->mu_);
                p->StartRefresh(lk);
              }
            }
          });

  lock.lock();
  if (timer_generation_ == timer_id) {
    refresh_timer_ = std::move(timer);
    return;
  }
  if (timer.valid()) timer.cancel();
}

void QueryPlan::StartRefresh(std::unique_lock<std::mutex>& lock) {
  if (state_ == RefreshState::kPending) return;
  state_ = RefreshState::kPending;
  std::uint64_t const op_id = ++op_generation_;
  std::weak_ptr<QueryPlan> plan = weak_from_this();
  internal::ImmutableOptions current = internal::SaveCurrentOptions();
  lock.unlock();

  future<void> op = refresh_fn_().then(
      [plan = std::move(plan), current = std::move(current)](
          future<StatusOr<google::bigtable::v2::PrepareQueryResponse>> f) {
        internal::OptionsSpan options_span(current);
        if (std::shared_ptr<QueryPlan> p = plan.lock()) {
          p->OnRefreshCompleted(f.get());
        }
      });

  lock.lock();
  if (op_generation_ == op_id && state_ == RefreshState::kPending) {
    refresh_op_ = std::move(op);
  }
}

void QueryPlan::OnRefreshCompleted(
    StatusOr<google::bigtable::v2::PrepareQueryResponse> response) {
  {
    std::unique_lock<std::mutex> lock(mu_);
    ++op_generation_;
    refresh_op_ = {};
    ++refresh_generation_;
    response_ = std::move(response);
    if (response_.ok()) {
      state_ = RefreshState::kDone;
      ScheduleRefresh(lock);
    } else {
      state_ = RefreshState::kBegin;
    }
  }
  cond_.notify_all();
}

void QueryPlan::Invalidate(Status status,
                           std::string const& invalid_query_plan_id) {
  future<void> prev_timer;
  {
    std::unique_lock<std::mutex> lock(mu_);
    // Avoid a late arrival causing a refresh of an already refreshed query
    // plan: only invalidate if the currently active plan matches the plan ID
    // reported as invalid.
    if (!response_.ok() ||
        response_->prepared_query() != invalid_query_plan_id) {
      return;
    }
    response_ = std::move(status);
    ++timer_generation_;
    prev_timer = std::move(refresh_timer_);
    if (state_ == RefreshState::kDone) state_ = RefreshState::kBegin;
  }
  if (prev_timer.valid()) prev_timer.cancel();
}

StatusOr<google::bigtable::v2::PrepareQueryResponse> QueryPlan::response() {
  std::unique_lock<std::mutex> lock(mu_);
  // When a prior reactive refresh fails (`state_ == kBegin` and
  // `!response_.ok()`), the next waiting `response()` caller intentionally
  // initiates a fresh single-flight attempt (`StartRefresh` sets
  // `state_ = kPending` under `mu_` so other woken waiters block again),
  // preserving the retry contract verified by
  // `QueryPlanMultithreadedTest.RefreshInvalidatedPlanAfterFailedRefresh`.
  cond_.wait(lock, [this] {
    return response_.ok() || state_ != RefreshState::kPending;
  });
  if (response_.ok()) return response_;
  std::uint64_t const gen = refresh_generation_;
  StartRefresh(lock);
  cond_.wait(lock, [this, gen] { return refresh_generation_ != gen; });
  return response_;
}

StatusOr<std::string> QueryPlan::prepared_query() {
  StatusOr<google::bigtable::v2::PrepareQueryResponse> data = response();
  if (!data.ok()) return data.status();
  return data->prepared_query();
}

StatusOr<google::bigtable::v2::ResultSetMetadata> QueryPlan::metadata() {
  StatusOr<google::bigtable::v2::PrepareQueryResponse> data = response();
  if (!data.ok()) return data.status();
  return data->metadata();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
