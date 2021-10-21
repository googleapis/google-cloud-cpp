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
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

TEST(PartitionOptionsTest, Regular) {
  PartitionOptions a;
  PartitionOptions b;
  EXPECT_EQ(a, b);

  a.partition_size_bytes = 1;
  EXPECT_NE(a, b);
  b.partition_size_bytes = 1;
  EXPECT_EQ(a, b);

  a.max_partitions = 1;
  EXPECT_NE(a, b);
  b.max_partitions = 1;
  EXPECT_EQ(a, b);

  auto copy = b;
  EXPECT_EQ(a, copy);
}

TEST(PartitionOptionsTest, Proto) {
  PartitionOptions po{1, 2};
  auto proto = spanner_internal::ToProto(po);
  EXPECT_EQ(*po.partition_size_bytes, proto.partition_size_bytes());
  EXPECT_EQ(*po.max_partitions, proto.max_partitions());
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
