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

#include "google/cloud/storage/internal/adaptive_read_size_estimator.h"
#include <algorithm>
#include <limits>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

// A recommendation is reused only if the parameters that affect it did not
// change.
bool SameModel(AdaptiveReadSettings const& a, AdaptiveReadSettings const& b) {
  return a.initial_window == b.initial_window && a.max_window == b.max_window &&
         a.growth_factor == b.growth_factor && a.max_drain == b.max_drain &&
         a.request_cost_bytes == b.request_cost_bytes;
}

}  // namespace

AdaptiveReadSizeEstimator::AdaptiveReadSizeEstimator(std::size_t max_buckets)
    : max_buckets_((std::max)(max_buckets, std::size_t{1})) {}

std::shared_ptr<AdaptiveReadSizeEstimator> AdaptiveReadSizeEstimator::Global() {
  // Never destroyed: adaptive reads may complete during static destruction.
  static auto* const kInstance = new std::shared_ptr<AdaptiveReadSizeEstimator>(
      std::make_shared<AdaptiveReadSizeEstimator>());
  return *kInstance;
}

void AdaptiveReadSizeEstimator::Record(std::string const& bucket,
                                       std::int64_t bytes_delivered,
                                       bool reached_eof) {
  Sample const sample{(std::max)(bytes_delivered, std::int64_t{0}),
                      reached_eof};
  std::lock_guard<std::mutex> lk(mu_);
  auto f = history_.find(bucket);
  if (f == history_.end()) {
    if (history_.size() >= max_buckets_) {
      history_.erase(lru_.back());
      lru_.pop_back();
    }
    lru_.push_front(bucket);
    f = history_.emplace(bucket, History{}).first;
    f->second.lru = lru_.begin();
    f->second.samples.reserve(kHistorySize);
  } else {
    lru_.splice(lru_.begin(), lru_, f->second.lru);
  }
  auto& h = f->second;
  if (h.samples.size() < kHistorySize) {
    h.samples.push_back(sample);
  } else {
    h.samples[h.next] = sample;
    h.next = (h.next + 1) % kHistorySize;
  }
  ++h.since_update;
}

std::int64_t AdaptiveReadSizeEstimator::Recommend(
    std::string const& bucket, AdaptiveReadSettings const& settings) {
  std::lock_guard<std::mutex> lk(mu_);
  auto f = history_.find(bucket);
  if (f == history_.end()) return settings.initial_window;
  lru_.splice(lru_.begin(), lru_, f->second.lru);
  auto& h = f->second;
  if (h.samples.size() < kMinSamples) return settings.initial_window;
  if (h.recommendation && h.since_update < kRecomputeInterval &&
      SameModel(h.recommendation_settings, settings)) {
    return *h.recommendation;
  }
  h.recommendation = Compute(h.samples, settings);
  h.recommendation_settings = settings;
  h.since_update = 0;
  return *h.recommendation;
}

std::size_t AdaptiveReadSizeEstimator::bucket_count() const {
  std::lock_guard<std::mutex> lk(mu_);
  return history_.size();
}

double AdaptiveReadSizeEstimator::SimulatedCost(
    std::int64_t bytes, bool reached_eof, std::int64_t first_window,
    AdaptiveReadSettings const& settings) {
  auto const request_cost = static_cast<double>(settings.request_cost_bytes);
  // The cost of the bytes left in the last window, @p remainder.
  auto remainder_cost = [&](std::int64_t remainder) {
    if (reached_eof) return 0.0;
    if (remainder <= settings.max_drain) return static_cast<double>(remainder);
    return static_cast<double>(settings.max_drain) + request_cost;
  };
  std::int64_t offset = 0;
  std::int64_t window = (std::max)(first_window, std::int64_t{1});
  double requests = 0;
  while (true) {
    requests += 1;
    auto const left = bytes - offset;
    if (window >= left) {
      return remainder_cost(window - left) + request_cost * (requests - 1);
    }
    offset += window;
    auto const next = NextAdaptiveWindowSize(window, settings);
    if (next <= window) {
      // The windows no longer grow, compute the rest directly.
      auto const rest = bytes - offset;
      auto const count = rest / window + (rest % window == 0 ? 0 : 1);
      requests += static_cast<double>(count);
      auto const remainder = (window - rest % window) % window;
      return remainder_cost(remainder) + request_cost * (requests - 1);
    }
    window = next;
  }
}

std::int64_t AdaptiveReadSizeEstimator::Compute(
    std::vector<Sample> const& samples, AdaptiveReadSettings const& settings) {
  if (settings.max_window < kMinCandidate) return settings.initial_window;
  auto best = settings.initial_window;
  auto best_cost = std::numeric_limits<double>::infinity();
  // Ties favor the smaller window, which is cheaper if the reads change.
  for (std::int64_t w = kMinCandidate; w <= settings.max_window; w *= 2) {
    double cost = 0;
    for (auto const& s : samples) {
      cost += SimulatedCost(s.bytes, s.reached_eof, w, settings);
    }
    if (cost < best_cost) {
      best = w;
      best_cost = cost;
    }
    if (w > settings.max_window / 2) break;
  }
  return best;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
