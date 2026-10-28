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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ADAPTIVE_READ_SIZE_ESTIMATOR_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ADAPTIVE_READ_SIZE_ESTIMATOR_H

#include "google/cloud/storage/internal/adaptive_object_read_source.h"
#include "google/cloud/storage/version.h"
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Chooses the size of the first window of adaptive reads.
 *
 * The estimator keeps, for each bucket, the last `kHistorySize` adaptive reads:
 * how many bytes the application used, and whether the read reached the end
 * of the data. Reads that start at the beginning of the object and reads that
 * start at a non-zero offset have separate histories, as applications often
 * use them for different access patterns (such as reading a file header versus
 * reading a data chunk). Once a history has `kMinSamples` reads, `Recommend()`
 * returns the first window (a power of two or a half-step `1.5 * 2^k` between
 * `kMinCandidate` and `max_window`) that would have minimized:
 *
 * @code
 * sum over reads ( bytes sent but not used
 *                  + request_cost_bytes * (requests - 1) )
 * @endcode
 *
 * Each read is replayed with the windows that start at the candidate size and
 * grow as configured. A read that stopped before the end of the data leaves a
 * remainder in its last window. A remainder up to `max_drain` bytes is read and
 * discarded, so it is sent but not used. A larger remainder is abandoned, which
 * is modeled as `max_drain` bytes sent but not used (the data already in
 * flight), plus `request_cost_bytes` for the connection that is closed.
 *
 * The history is bounded: at most `max_buckets` buckets, the least recently
 * used bucket is evicted first. All the functions are thread-safe.
 *
 * The estimator is usually shared by all the clients in the process (see
 * `Global()`): some libraries create a new `Client` for each object they read,
 * so a per-client history would always be empty.
 */
class AdaptiveReadSizeEstimator {
 public:
  /// The number of reads kept per bucket.
  static std::size_t constexpr kHistorySize = 256;
  /// The number of reads needed before the estimator recommends a window.
  static std::size_t constexpr kMinSamples = 32;
  /// The smallest first window recommended.
  static std::int64_t constexpr kMinCandidate = 256 * 1024;
  /// The default number of buckets with a history.
  static std::size_t constexpr kDefaultMaxBuckets = 64;
  /// The recommendation is recomputed after this many new reads.
  static std::size_t constexpr kRecomputeInterval = 16;

  explicit AdaptiveReadSizeEstimator(
      std::size_t max_buckets = kDefaultMaxBuckets);

  /// The estimator shared by all the clients in the process.
  static std::shared_ptr<AdaptiveReadSizeEstimator> Global();

  /**
   * Records an adaptive read from @p bucket.
   *
   * @param bytes_delivered the bytes returned to the application.
   * @param reached_eof true if the read returned all the requested data, that
   *     is, until the end of the object.
   * @param from_offset true if the read started at a positive offset.
   */
  void Record(std::string const& bucket, std::int64_t bytes_delivered,
              bool reached_eof, bool from_offset = false);

  /// Returns the size of the first window for a read from @p bucket.
  std::int64_t Recommend(std::string const& bucket,
                         AdaptiveReadSettings const& settings,
                         bool from_offset = false);

  /// The number of buckets with a history.
  std::size_t bucket_count() const;

  /**
   * The cost of a read, in bytes, if its first window was @p first_window.
   *
   * @param bytes the bytes used by the application.
   * @param reached_eof true if the read reached the end of the data, in that
   *     case the last window is clamped to the end of the data.
   */
  static double SimulatedCost(std::int64_t bytes, bool reached_eof,
                              std::int64_t first_window,
                              AdaptiveReadSettings const& settings);

 private:
  struct Sample {
    std::int64_t bytes;
    bool reached_eof;
  };
  struct History {
    std::vector<Sample> samples;
    // The next position to overwrite once `samples` is full.
    std::size_t next = 0;
    std::size_t since_update = 0;
    std::optional<std::int64_t> recommendation;
    AdaptiveReadSettings recommendation_settings{};
  };
  struct BucketEntry {
    // Indexed by `from_offset`: reads from the start of the object (0) and
    // reads from a positive offset (1).
    History by_origin[2];
    std::list<std::string>::iterator lru;
  };

  static std::int64_t Compute(std::vector<Sample> const& samples,
                              AdaptiveReadSettings const& settings);

  std::size_t const max_buckets_;
  mutable std::mutex mu_;
  // Most recently used first.
  std::list<std::string> lru_;                            // GUARDED_BY(mu_)
  std::unordered_map<std::string, BucketEntry> history_;  // GUARDED_BY(mu_)
};

/**
 * Overrides the estimator used by adaptive reads.
 *
 * This is only intended for testing. Adaptive reads use
 * `AdaptiveReadSizeEstimator::Global()` when the option is not set.
 */
struct AdaptiveReadSizeEstimatorOption {
  using Type = std::shared_ptr<AdaptiveReadSizeEstimator>;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ADAPTIVE_READ_SIZE_ESTIMATOR_H
