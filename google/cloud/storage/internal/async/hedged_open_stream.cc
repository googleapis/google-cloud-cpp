// Copyright 2026 Google LLC
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

#include "google/cloud/storage/internal/async/hedged_open_stream.h"
#include "google/cloud/internal/call_context.h"
#include "google/cloud/internal/make_status.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

std::chrono::milliseconds constexpr kHedgeRetryMinDelay{10};

class HedgedOpenOperation
    : public std::enable_shared_from_this<HedgedOpenOperation> {
 public:
  HedgedOpenOperation(
      CompletionQueue cq, OpenStreamFactory factory,
      google::storage::v2::BidiReadObjectRequest initial_request,
      std::shared_ptr<storage::AsyncRetryPolicy> retry_policy,
      HedgedOpenParams params,
      std::shared_ptr<storage::internal::HedgeLimiter> limiter,
      std::shared_ptr<storage::internal::HedgedReadMetrics> metrics,
      google::cloud::internal::ImmutableOptions options)
      : cq_(std::move(cq)),
        factory_(std::move(factory)),
        initial_request_(std::move(initial_request)),
        retry_policy_(std::move(retry_policy)),
        hedge_delay_(
            (std::max)(std::chrono::milliseconds::zero(), params.delay)),
        max_hedges_((std::max)(0, params.max_hedges)),
        limiter_(std::move(limiter)),
        metrics_(std::move(metrics)),
        call_context_(std::move(options)) {}

  future<StatusOr<OpenStreamResult>> Start() {
    std::weak_ptr<HedgedOpenOperation> weak = shared_from_this();
    result_promise_ = promise<StatusOr<OpenStreamResult>>([weak] {
      if (std::shared_ptr<HedgedOpenOperation> self = weak.lock()) {
        google::cloud::internal::ScopedCallContext scope(self->call_context_);
        self->OnCallerCancel();
      }
    });
    future<StatusOr<OpenStreamResult>> result_future =
        result_promise_.get_future();

    active_attempts_ = 1;
    attempts_.emplace_back(future<void>{});
    bool const arm_timer = CanHedge();

    AttachAttempt(0, StartAttempt(0));
    if (arm_timer) ArmTimer(hedge_delay_);
    return result_future;
  }

 private:
  struct HedgeSlot {
    std::shared_ptr<storage::internal::HedgeLimiter> limiter;

    HedgeSlot() = default;
    explicit HedgeSlot(std::shared_ptr<storage::internal::HedgeLimiter> l)
        : limiter(std::move(l)) {}
    ~HedgeSlot() { Release(); }
    HedgeSlot(HedgeSlot&&) noexcept = default;
    HedgeSlot& operator=(HedgeSlot&&) = delete;
    HedgeSlot(HedgeSlot const&) = delete;
    HedgeSlot& operator=(HedgeSlot const&) = delete;

    void Release() {
      if (limiter) std::exchange(limiter, nullptr)->ReleaseHedgeSlot();
    }
  };

  struct Outstanding {
    future<void> timer;
    std::vector<std::optional<future<void>>> attempts;

    void Cancel() {
      CancelIfValid(timer);
      for (std::optional<future<void>>& pending : attempts) {
        if (pending) CancelIfValid(*pending);
      }
    }
  };

  static void CancelIfValid(future<void>& f) {
    if (f.valid()) f.cancel();
  }

  static Status CancelledOpenError() {
    return google::cloud::internal::CancelledError("Open operation cancelled",
                                                   GCP_ERROR_INFO());
  }

  bool CanHedge() const {
    return hedges_dispatched_ < max_hedges_ &&
           (!limiter_ || !limiter_->IsTotalBudgetSpent());
  }

  future<void> FinishAttemptLocked(std::size_t idx) {
    --active_attempts_;
    if (idx < attempts_.size() && attempts_[idx].has_value()) {
      return *std::exchange(attempts_[idx], std::nullopt);
    }
    return future<void>{};
  }

  Outstanding DrainOutstandingLocked() {
    ++timer_generation_;
    return Outstanding{std::move(timer_future_), std::exchange(attempts_, {})};
  }

  void Resolve(Outstanding& outstanding, StatusOr<OpenStreamResult> result) {
    outstanding.Cancel();
    result_promise_.set_value(std::move(result));
  }

  future<void> StartAttempt(std::size_t idx) {
    HedgeSlot slot = idx == 0 ? HedgeSlot{} : HedgeSlot{limiter_};
    google::cloud::internal::ScopedCallContext scope(call_context_);
    std::shared_ptr<HedgedOpenOperation> self = shared_from_this();
    return factory_(initial_request_)
        .then([self = std::move(self), slot = std::move(slot),
               idx](future<StatusOr<OpenStreamResult>> f) mutable {
          slot.Release();
          self->OnAttemptDone(idx, f.get());
        });
  }

  void AttachAttempt(std::size_t idx, future<void> attempt) {
    if (!attempt.valid() || attempt.is_ready()) return;
    bool cancel_now = false;
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (resolved_ || caller_cancelled_) {
        cancel_now = true;
      } else if (idx < attempts_.size() && attempts_[idx].has_value()) {
        attempts_[idx] = std::move(attempt);
      }
    }
    if (cancel_now) CancelIfValid(attempt);
  }

  void ArmTimer(std::chrono::milliseconds delay) {
    std::uint64_t gen = 0;
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (resolved_ || caller_cancelled_) return;
      gen = ++timer_generation_;
    }
    std::weak_ptr<HedgedOpenOperation> weak = shared_from_this();
    future<void> timer = cq_.MakeRelativeTimer(delay).then(
        [weak, gen](future<StatusOr<std::chrono::system_clock::time_point>> f) {
          if (std::shared_ptr<HedgedOpenOperation> self = weak.lock()) {
            self->OnTimer(gen, f.get().ok());
          }
        });
    if (!timer.valid() || timer.is_ready()) return;
    bool cancel_now = false;
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (resolved_ || caller_cancelled_) {
        cancel_now = true;
      } else if (gen == timer_generation_) {
        timer_future_ = std::move(timer);
      }
    }
    if (cancel_now) CancelIfValid(timer);
  }

  void OnTimer(std::uint64_t gen, bool expired) {
    if (!expired) return;
    google::cloud::internal::ScopedCallContext scope(call_context_);

    bool dispatch_hedge = false;
    std::size_t idx = 0;
    bool arm_next_timer = false;
    std::chrono::milliseconds next_delay = hedge_delay_;
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (resolved_ || caller_cancelled_ || gen != timer_generation_ ||
          hedges_dispatched_ >= max_hedges_) {
        return;
      }
      ++timer_generation_;
      timer_future_ = future<void>{};
      bool const acquired = !limiter_ || limiter_->TryAcquireHedgeToken();
      if (acquired) {
        dispatch_hedge = true;
        ++active_attempts_;
        ++hedges_dispatched_;
        idx = attempts_.size();
        attempts_.emplace_back(future<void>{});
      }
      if (CanHedge()) {
        arm_next_timer = true;
        next_delay = acquired ? hedge_delay_
                              : (std::max)(hedge_delay_, kHedgeRetryMinDelay);
      }
    }

    // If a sibling won after unlocking `mu_`, `AttachAttempt` cancels it.
    if (dispatch_hedge) {
      if (metrics_) metrics_->OnHedgeDispatched();
      AttachAttempt(idx, StartAttempt(idx));
    }
    if (arm_next_timer) ArmTimer(next_delay);
  }

  void OnAttemptDone(std::size_t idx, StatusOr<OpenStreamResult> result) {
    google::cloud::internal::ScopedCallContext scope(call_context_);
    if (!result) return OnAttemptError(idx, std::move(result).status());
    future<void> self_attempt;
    Outstanding outstanding;
    bool won = false;
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (!resolved_ && !caller_cancelled_) {
        won = true;
        self_attempt = FinishAttemptLocked(idx);
        resolved_ = true;
        outstanding = DrainOutstandingLocked();
      }
    }
    if (!won) {
      if (result->stream) result->stream->Cancel();
      OnAttemptError(idx, CancelledOpenError());
      return;
    }
    if (idx > 0 && metrics_) metrics_->OnHedgeWon();
    Resolve(outstanding, *std::move(result));
  }

  Status SelectFinalErrorLocked() const {
    // `RetryLoopCancelled` preserves the last transient status code when
    // cancelled during backoff, so map caller-cancelled races to `kCancelled`.
    if (caller_cancelled_) {
      return primary_error_.code() == StatusCode::kCancelled
                 ? primary_error_
                 : CancelledOpenError();
    }
    if (!permanent_error_.ok()) return permanent_error_;
    return primary_error_;
  }

  void OnAttemptError(std::size_t idx, Status status) {
    // Unlike mid-stream sync `HedgedObjectReadSource`, every `Open()` attempt
    // sends the identical initial request, so an authoritative permanent
    // rejection (`kNotFound`, `kPermissionDenied`, ...) from *any* attempt
    // aborts stalled siblings immediately. `kCancelled` is excluded because
    // per-stream stall timeouts surface as `kCancelled` on that attempt alone.
    bool const permanent_error = status.code() != StatusCode::kCancelled &&
                                 retry_policy_ &&
                                 retry_policy_->IsPermanentFailure(status);
    future<void> self_attempt;
    Outstanding outstanding;
    Status final_error;
    bool resolve_now = false;
    {
      std::lock_guard<std::mutex> lock(mu_);
      self_attempt = FinishAttemptLocked(idx);
      if (resolved_) return;
      if (permanent_error && permanent_error_.ok()) permanent_error_ = status;
      if (idx == 0) primary_error_ = std::move(status);
      // Each attempt runs its own `AsyncRetryLoop`, so `active_attempts_ == 0`
      // means every launched attempt already exhausted its retry policy; do
      // not wait for un-fired hedge timers to double the retry budget.
      if (active_attempts_ == 0 ||
          (!caller_cancelled_ && !permanent_error_.ok())) {
        resolve_now = true;
        resolved_ = true;
        final_error = SelectFinalErrorLocked();
        outstanding = DrainOutstandingLocked();
      }
    }
    if (resolve_now) Resolve(outstanding, std::move(final_error));
  }

  void OnCallerCancel() {
    Outstanding outstanding;
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (resolved_ || caller_cancelled_) return;
      caller_cancelled_ = true;
      outstanding = DrainOutstandingLocked();
    }
    outstanding.Cancel();
  }

  CompletionQueue cq_;
  OpenStreamFactory factory_;
  google::storage::v2::BidiReadObjectRequest initial_request_;
  std::shared_ptr<storage::AsyncRetryPolicy> retry_policy_;
  std::chrono::milliseconds const hedge_delay_;
  int const max_hedges_;
  std::shared_ptr<storage::internal::HedgeLimiter> limiter_;
  std::shared_ptr<storage::internal::HedgedReadMetrics> metrics_;
  google::cloud::internal::CallContext call_context_;
  promise<StatusOr<OpenStreamResult>> result_promise_{null_promise_t{}};

  std::mutex mu_;
  bool resolved_ = false;
  bool caller_cancelled_ = false;
  int active_attempts_ = 0;
  int hedges_dispatched_ = 0;
  std::uint64_t timer_generation_ = 0;
  Status permanent_error_;
  Status primary_error_;
  future<void> timer_future_;
  std::vector<std::optional<future<void>>> attempts_;
};

}  // namespace

future<StatusOr<OpenStreamResult>> HedgedOpenStream(
    CompletionQueue cq, OpenStreamFactory factory,
    google::storage::v2::BidiReadObjectRequest initial_request,
    std::shared_ptr<storage::AsyncRetryPolicy> retry_policy,
    HedgedOpenParams params,
    std::shared_ptr<storage::internal::HedgeLimiter> limiter,
    std::shared_ptr<storage::internal::HedgedReadMetrics> metrics,
    google::cloud::internal::ImmutableOptions options) {
  return std::make_shared<HedgedOpenOperation>(
             std::move(cq), std::move(factory), std::move(initial_request),
             std::move(retry_policy), params, std::move(limiter),
             std::move(metrics), std::move(options))
      ->Start();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
