// Copyright 2021 Google LLC
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

#include "google/cloud/internal/algorithm.h"
#include <gmock/gmock.h>
#include <string>
#include <vector>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

TEST(Algorithm, StringContains) {
  std::string v = "abcde";
  EXPECT_TRUE(Contains(v, 'c'));
  EXPECT_FALSE(Contains(v, 'z'));
}

TEST(Algorithm, ArrayContains) {
  std::string v[] = {"foo", "bar", "baz"};
  EXPECT_TRUE(Contains(v, "foo"));
  EXPECT_FALSE(Contains(v, "OOPS"));
}

TEST(Algorithm, VectorContains) {
  std::vector<std::string> v = {"foo", "bar", "baz"};
  EXPECT_TRUE(Contains(v, "foo"));
  EXPECT_FALSE(Contains(v, "OOPS"));
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
