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

#include "google/cloud/bigtable/internal/mutate_rows_limiter.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/grpc_opentelemetry.h"
#include "google/cloud/internal/opentelemetry.h"
#include "absl/time/time.h"
#include <algorithm>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// clamp value to the range [min, max]
template <typename T>
T Clamp(T value, T min, T max) {
  return (std::min)(max, (std::max)(min, value));
}

auto constexpr kMinFactor = .7;
auto constexpr kMaxFactor = 1.3;
auto constexpr kInitialPeriod = std::chrono::milliseconds(50);
auto constexpr kMinPeriod = std::chrono::microseconds(10);
auto constexpr kMaxPeriod = std::chrono::seconds(10);

}  // namespace

void ThrottlingMutateRowsLimiter::Acquire() {
  auto wait = limiter_.acquire(1);
  throttled_since_last_update_ =
      throttled_since_last_update_ || wait != Clock::duration::zero();
  on_wait_(wait);
}

future<void> ThrottlingMutateRowsLimiter::AsyncAcquire() {
  auto wait = limiter_.acquire(1);
  throttled_since_last_update_ =
      throttled_since_last_update_ || wait != Clock::duration::zero();
  return async_on_wait_(wait);
}

void ThrottlingMutateRowsLimiter::Update(
    google::bigtable::v2::MutateRowsResponse const& response) {
  if (!response.has_rate_limit_info()) return;
  auto const now = clock_->Now();
  if (now < next_update_) return;
  auto const& info = response.rate_limit_info();
  next_update_ = now + std::chrono::duration_cast<Clock::duration>(
                           std::chrono::seconds(info.period().seconds()) +
                           std::chrono::nanoseconds(info.period().nanos()));

  // The effective QPS can lag behind the max QPS allowed by the rate limiter.
  // In such a case, we should not keep increasing the max QPS allowed. We
  // should only increase the ceiling if we are actually hitting that ceiling.
  if (info.factor() > 1 && !throttled_since_last_update_) return;
  throttled_since_last_update_ = false;

  auto factor = Clamp(info.factor(), min_factor_, max_factor_);
  auto const period = Clamp(
      std::chrono::duration_cast<Clock::duration>(limiter_.period() / factor),
      min_period_, max_period_);
  limiter_.set_period(period);
}

std::shared_ptr<MutateRowsLimiter> MakeMutateRowsLimiter(CompletionQueue cq,
                                                         Options options) {
  if (!options.get<bigtable::experimental::BulkApplyThrottlingOption>()) {
    return std::make_shared<NoopMutateRowsLimiter>();
  }
  using duration = ThrottlingMutateRowsLimiter::Clock::duration;
  std::function<void(duration)> sleeper = [](duration d) {
    std::this_thread::sleep_for(d);
  };
  if (internal::Contains(options.get<LoggingComponentsOption>(), "rpc")) {
    sleeper = [sleeper = std::move(sleeper)](duration d) {
      if (d != duration::zero()) {
        GCP_LOG(DEBUG) << "Throttling BulkApply for " << absl::FromChrono(d);
      }
      sleeper(d);
    };
  }
  sleeper = internal::MakeTracedSleeper(
      options, std::move(sleeper), "gl-cpp.bigtable.bulk_apply_throttling");
  auto async_sleeper = [cq = std::move(cq),
                        o = std::move(options)](duration d) mutable {
    return internal::TracedAsyncBackoff(cq, o, d,
                                        "gl-cpp.bigtable.bulk_apply_throttling")
        .then([](auto f) { (void)f.get(); });
  };
  return std::make_shared<ThrottlingMutateRowsLimiter>(
      std::make_shared<internal::SteadyClock>(), std::move(sleeper),
      std::move(async_sleeper), kInitialPeriod, kMinPeriod, kMaxPeriod,
      kMinFactor, kMaxFactor);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
