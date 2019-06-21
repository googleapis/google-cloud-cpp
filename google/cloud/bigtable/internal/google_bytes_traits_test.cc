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

#include "google/cloud/bigtable/internal/google_bytes_traits.h"
#include <gtest/gtest.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
namespace {

TEST(GoogleBytesTraitsTest, ConsecutiveRowKeys) {
  EXPECT_FALSE(ConsecutiveRowKeys("a", "a"));
  EXPECT_FALSE(ConsecutiveRowKeys("b", "a"));
  EXPECT_FALSE(ConsecutiveRowKeys("a", "c"));
  EXPECT_FALSE(ConsecutiveRowKeys("a", std::string("a\1", 2)));
  EXPECT_TRUE(ConsecutiveRowKeys("a", std::string("a\0", 2)));
}

}  // namespace
}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
