// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_MUTATE_ROWS_LIMITER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_MUTATE_ROWS_LIMITER_H

#include "google/cloud/bigtable/internal/rate_limiter.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/clock.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include "google/bigtable/v2/bigtable.pb.h"
#include <chrono>
#include <functional>
#include <memory>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// A Bigtable-specific wrapper over the more generic `RateLimiter`
class MutateRowsLimiter {
 public:
  virtual ~MutateRowsLimiter() = default;
  virtual void Acquire() = 0;
  virtual future<void> AsyncAcquire() = 0;
  virtual void Update(
      google::bigtable::v2::MutateRowsResponse const& response) = 0;
};

class NoopMutateRowsLimiter : public MutateRowsLimiter {
 public:
  void Acquire() override {}
  future<void> AsyncAcquire() override { return make_ready_future(); }
  void Update(google::bigtable::v2::MutateRowsResponse const&) override {}
};

class ThrottlingMutateRowsLimiter : public MutateRowsLimiter {
 public:
  using Clock = RateLimiter::Clock;
  template <typename Rep1, typename Period1, typename Rep2, typename Period2,
            typename Rep3, typename Period3>
  explicit ThrottlingMutateRowsLimiter(
      std::shared_ptr<Clock> clock,
      std::function<void(Clock::duration)> on_wait,
      std::function<future<void>(Clock::duration)> async_on_wait,
      std::chrono::duration<Rep1, Period1> initial_period,
      std::chrono::duration<Rep2, Period2> min_period,
      std::chrono::duration<Rep3, Period3> max_period, double min_factor,
      double max_factor)
      : clock_(std::move(clock)),
        limiter_(clock_, initial_period),
        on_wait_(std::move(on_wait)),
        async_on_wait_(std::move(async_on_wait)),
        next_update_(clock_->Now()),
        min_period_(std::chrono::duration_cast<Clock::duration>(min_period)),
        max_period_(std::chrono::duration_cast<Clock::duration>(max_period)),
        min_factor_(min_factor),
        max_factor_(max_factor) {}

  void Acquire() override;

  future<void> AsyncAcquire() override;

  /**
   * As specified in:
   * https://cloud.google.com/bigtable/docs/reference/data/rpc/google.bigtable.v2#google.bigtable.v2.RateLimitInfo
   */
  void Update(
      google::bigtable::v2::MutateRowsResponse const& response) override;

  // Exposed for testing
  Clock::duration period() const { return limiter_.period(); }

 private:
  std::shared_ptr<Clock> clock_;
  RateLimiter limiter_;
  std::function<void(Clock::duration)> on_wait_;
  std::function<future<void>(Clock::duration)> async_on_wait_;
  bool throttled_since_last_update_ = false;
  Clock::time_point next_update_;
  Clock::duration min_period_;
  Clock::duration max_period_;
  double min_factor_;
  double max_factor_;
};

std::shared_ptr<MutateRowsLimiter> MakeMutateRowsLimiter(CompletionQueue cq,
                                                         Options options);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_MUTATE_ROWS_LIMITER_H
