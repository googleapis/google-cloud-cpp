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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_METRICS_HISTOGRAMS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_METRICS_HISTOGRAMS_H

#include "google/cloud/version.h"
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Build the histogram bucket boundaries for a gRPC latency histogram.
 *
 * Cloud Monitoring only supports up to 200 buckets per histogram, we have to
 * choose them carefully. The default uses [0s, 5s) as the first bucket. Putting
 * all the interesting data in one bucket is not great.
 */
std::vector<double> MakeLatencyHistogramBoundaries();

/**
 * Build the histogram bucket boundaries for a gRPC request / response size
 * histogram.
 *
 * Cloud Monitoring only supports up to 200 buckets per histogram, we have to
 * choose them carefully. The default wastes too many buckets with tiny sizes,
 * e.g., [0, 5) bytes. We create a histogram with buckets of 128KiB until they
 * get to 4MiB, and then grow the bucket sizes exponentially.
 */
std::vector<double> MakeSizeHistogramBoundaries();

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_METRICS_HISTOGRAMS_H
