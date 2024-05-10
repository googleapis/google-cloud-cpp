// Copyright 2024 Google LLC
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

#include "google/cloud/storage/internal/grpc/metrics_histograms.h"
#include <chrono>
#include <cstdint>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::vector<double> MakeLatencyHistogramBoundaries() {
  // The histogram is in seconds (and `double`s). We will use `std::chrono::`
  // types to compute the boundaries, and this type (`dseconds`) to convert to
  // floating point seconds.
  using dseconds = std::chrono::duration<double, std::ratio<1>>;
  std::vector<double> boundaries;
  auto boundary = std::chrono::milliseconds(0);
  auto increment = std::chrono::milliseconds(2);
  // For the first 100ms use 2ms buckets. We need higher resolution in this area
  // for uploads and downloads in the 100 KiB range, which are fairly common.
  for (int i = 0; i != 50; ++i) {
    boundaries.push_back(
        std::chrono::duration_cast<dseconds>(boundary).count());
    boundary += increment;
  }
  // The remaining buckets are 10ms wide, and then 20ms, and so forth. We stop
  // at 5 minutes.
  increment = std::chrono::milliseconds(10);
  for (int i = 0; i != 150 && boundary <= std::chrono::minutes(5); ++i) {
    boundaries.push_back(
        std::chrono::duration_cast<dseconds>(boundary).count());
    if (i != 0 && i % 10 == 0) increment *= 2;
    boundary += increment;
  }
  return boundaries;
}

std::vector<double> MakeSizeHistogramBoundaries() {
  auto constexpr kKiB = std::int64_t{1024};
  auto constexpr kMiB = 1024 * kKiB;
  auto constexpr kGiB = 1024 * kMiB;

  // Cloud Monitoring only supports up to 200 buckets per histogram, we have to
  // choose them carefully.
  std::vector<double> boundaries;
  std::int64_t boundary = 0;
  std::int64_t increment = 128 * kKiB;
  while (boundaries.size() < 200 && boundary <= 16 * kGiB) {
    boundaries.push_back(static_cast<double>(boundary));
    boundary += increment;
    // Track sizes on 128 KiB increments up to 4 MiB, then grow the increments
    // exponentially.
    if (boundary >= 4 * kMiB) increment *= 2;
  }
  return boundaries;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
