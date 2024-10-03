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
#include <gmock/gmock.h>
#include <algorithm>
#include <numeric>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::ElementsAreArray;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::Le;
using ::testing::SizeIs;

TEST(GrpcMetricsExporter, MakeLatencyHistogramBoundaries) {
  auto const boundaries = MakeLatencyHistogramBoundaries();
  // First verify the histogram meets the size constraints imposed by GCM
  // (Google Cloud Monitoring)
  ASSERT_THAT(boundaries, Not(IsEmpty()));
  ASSERT_THAT(boundaries, SizeIs(Le(200U)));
  // The boundaries should be sorted in increasing value.
  auto sorted = boundaries;
  std::sort(sorted.begin(), sorted.end());
  EXPECT_THAT(boundaries, ElementsAreArray(sorted.begin(), sorted.end()));
  // The smallest interval should be greater than a millisecond.
  std::vector<double> diff;
  std::adjacent_difference(boundaries.begin(), boundaries.end(),
                           std::back_inserter(diff));
  ASSERT_THAT(diff, Not(IsEmpty()));
  EXPECT_THAT(*std::min_element(std::next(diff.begin()), diff.end()),
              Ge(0.001));
  // We want the histogram to stop at about 5 minutes (300s).
  ASSERT_THAT(boundaries.back(), Le(300));
}

TEST(GrpcMetricsExporter, MakeSizeHistogramBoundaries) {
  auto const boundaries = MakeSizeHistogramBoundaries();
  // First verify the histogram meets the size constraints imposed by GCM
  // (Google Cloud Monitoring)
  ASSERT_THAT(boundaries, Not(IsEmpty()));
  ASSERT_THAT(boundaries, SizeIs(Le(200U)));
  // The boundaries should be sorted in increasing value.
  auto sorted = boundaries;
  std::sort(sorted.begin(), sorted.end());
  EXPECT_THAT(boundaries, ElementsAreArray(sorted.begin(), sorted.end()));
  // The smallest interval should be about 128 KiB.
  std::vector<double> diff;
  std::adjacent_difference(boundaries.begin(), boundaries.end(),
                           std::back_inserter(diff));
  ASSERT_THAT(diff, Not(IsEmpty()));
  EXPECT_THAT(*std::min_element(std::next(diff.begin()), diff.end()),
              Ge(128 * 1024.0));

  // We want the histogram to stop at about 16 GiB.
  ASSERT_THAT(boundaries.back(), Le(16.0 * 1024.0 * 1024.0 * 1024.0));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
