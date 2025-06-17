// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_PARTITION_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_PARTITION_OPTIONS_H

#include "google/cloud/spanner/version.h"
#include "google/cloud/optional.h"
#include "google/cloud/options.h"
#include "absl/types/optional.h"
#include "google/spanner/v1/spanner.pb.h"
#include <cstdint>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Options passed to `Client::PartitionRead` or `Client::PartitionQuery`.
 *
 * @deprecated Use [`Options`](@ref google::cloud::Options) instead,
 *     and set (as needed)
 *     [`PartitionSizeOption`](
 *     @ref google::cloud::spanner::PartitionSizeOption),
 *     [`PartitionsMaximumOption`](
 *     @ref google::cloud::spanner::PartitionsMaximumOption), or
 *     [`PartitionDataBoostOption`](
 *     @ref google::cloud::spanner::PartitionDataBoostOption).
 *
 * See documentation in [spanner.proto][spanner-proto].
 *
 * [spanner-proto]:
 * https://github.com/googleapis/googleapis/blob/70147caca58ebf4c8cd7b96f5d569a72723e11c1/google/spanner/v1/spanner.proto#L758
 */
struct PartitionOptions {
  /**
   * The desired data size for each partition generated.
   *
   * The default for this option is currently 1 GiB.  This is only a hint. The
   * actual size of each partition may be smaller or larger than this size
   * request.
   */
  absl::optional<std::int64_t> partition_size_bytes;

  /**
   * The desired maximum number of partitions to return.
   *
   * For example, this may be set to the number of workers available.  The
   * default for this option is currently 10,000. The maximum value is
   * currently 200,000. This is only a hint.  The actual number of partitions
   * returned may be smaller or larger than this maximum count request.
   */
  absl::optional<std::int64_t> max_partitions;

  /**
   * Use "data boost" in the returned partitions.
   *
   * If true, the requests from the subsequent partitioned `Client::Read()`
   * and `Client::ExecuteQuery()` calls will be executed using the independent
   * compute resources of Cloud Spanner Data Boost.
   */
  bool data_boost = false;
};

inline bool operator==(PartitionOptions const& a, PartitionOptions const& b) {
  return a.partition_size_bytes == b.partition_size_bytes &&
         a.max_partitions == b.max_partitions && a.data_boost == b.data_boost;
}

inline bool operator!=(PartitionOptions const& a, PartitionOptions const& b) {
  return !(a == b);
}

/// Converts `PartitionOptions` to common `Options`.
Options ToOptions(PartitionOptions const&);

/// Converts common `Options` to `PartitionOptions`.
PartitionOptions ToPartitionOptions(Options const&);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner

// Internal implementation details that callers should not use.
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
google::spanner::v1::PartitionOptions ToProto(spanner::PartitionOptions const&);
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_PARTITION_OPTIONS_H
