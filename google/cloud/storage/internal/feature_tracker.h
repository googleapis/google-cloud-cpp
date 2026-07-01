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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_FEATURE_TRACKER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_FEATURE_TRACKER_H

#include "google/cloud/storage/version.h"
#include "google/cloud/options.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

inline constexpr auto kFeatureTrackerHeaderName = "x-goog-storage-cpp-features";

// Tracked features represented as bit positions in a bitmask.
enum class TrackedFeature : std::uint32_t {
  // Operation-Driven Optimizations
  kMultiStreamInMRD = 0,
  kPCU = 1,

  // Configuration-Driven Options
  kGrpcDirectPathEnforced = 2,
  kJsonReads = 3,
};

// Converts a feature bitmask to a Base64-encoded string, stripping leading
// zero bytes to maintain compact representation.
std::string EncodeFeatureTrackerBitmask(std::uint32_t mask);

// A thread-safe state object that can be shared between operation connections
// (like ObjectDescriptorImpl) and RPC factories (like OpenStreamFactory)
// to track which features are adopted during an operation.
class FeatureTracker {
 public:
  FeatureTracker() = default;
  explicit FeatureTracker(std::uint32_t initial) : mask_(initial) {}

  void RegisterFeature(TrackedFeature feature) {
    mask_.fetch_or(1U << static_cast<std::uint32_t>(feature),
                   std::memory_order_relaxed);
  }

  std::uint32_t GetMask() const {
    return mask_.load(std::memory_order_relaxed);
  }

  std::string HeaderValue() const {
    return EncodeFeatureTrackerBitmask(GetMask());
  }

 private:
  std::atomic<std::uint32_t> mask_{0};
};

struct FeatureTrackerOption {
  using Type = std::shared_ptr<FeatureTracker>;
};

// Evaluates client configuration options, creates a shared FeatureTracker
// initialized with configuration-driven feature flags (if any), and stores it
// into the Options list under FeatureTrackerOption.
Options SetupFeatureTracker(Options opts);

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_FEATURE_TRACKER_H
