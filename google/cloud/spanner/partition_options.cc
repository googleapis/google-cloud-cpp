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

#include "google/cloud/spanner/partition_options.h"
#include "google/cloud/spanner/options.h"

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

Options ToOptions(PartitionOptions const& po) {
  Options opts;
  if (po.partition_size_bytes) {
    opts.set<PartitionSizeOption>(*po.partition_size_bytes);
  }
  if (po.max_partitions) {
    opts.set<PartitionsMaximumOption>(*po.max_partitions);
  }
  return opts;
}

PartitionOptions ToPartitionOptions(Options const& opts) {
  PartitionOptions po;
  if (opts.has<PartitionSizeOption>()) {
    po.partition_size_bytes = opts.get<PartitionSizeOption>();
  }
  if (opts.has<PartitionsMaximumOption>()) {
    po.max_partitions = opts.get<PartitionsMaximumOption>();
  }
  return po;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner

namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

google::spanner::v1::PartitionOptions ToProto(
    spanner::PartitionOptions const& po) {
  google::spanner::v1::PartitionOptions proto;
  if (po.max_partitions) proto.set_max_partitions(*po.max_partitions);
  if (po.partition_size_bytes) {
    proto.set_partition_size_bytes(*po.partition_size_bytes);
  }
  return proto;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
