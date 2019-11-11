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

#include "google/cloud/spanner/partition_options.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

namespace internal {

google::spanner::v1::PartitionOptions ToProto(PartitionOptions const& po) {
  google::spanner::v1::PartitionOptions proto;
  if (po.max_partitions) proto.set_max_partitions(*po.max_partitions);
  if (po.partition_size_bytes) {
    proto.set_partition_size_bytes(*po.partition_size_bytes);
  }
  return proto;
}

}  // namespace internal

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
