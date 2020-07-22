// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
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
#include "absl/types/optional.h"
#include <google/spanner/v1/spanner.pb.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * Options passed to `Client::PartitionRead` or `Client::PartitionQuery`.
 *
 * See documentation in [spanner.proto][spanner-proto].
 *
 * [spanner-proto]:
 * https://github.com/googleapis/googleapis/blob/0ed34e9fdf601dfc37eb24c40e17495b86771ff4/google/spanner/v1/spanner.proto#L651
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
};

inline bool operator==(PartitionOptions const& a, PartitionOptions const& b) {
  return a.partition_size_bytes == b.partition_size_bytes &&
         a.max_partitions == b.max_partitions;
}

inline bool operator!=(PartitionOptions const& a, PartitionOptions const& b) {
  return !(a == b);
}

namespace internal {
google::spanner::v1::PartitionOptions ToProto(PartitionOptions const&);
}  // namespace internal

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_PARTITION_OPTIONS_H
