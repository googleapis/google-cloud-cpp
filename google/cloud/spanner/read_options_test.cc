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

#include "google/cloud/spanner/read_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

TEST(ReadOptionsTest, Equality) {
  ReadOptions test_options_0{};
  ReadOptions test_options_1{};
  EXPECT_EQ(test_options_0, test_options_1);
  test_options_0.index_name = "secondary";
  EXPECT_NE(test_options_0, test_options_1);
  test_options_1.index_name = "secondary";
  EXPECT_EQ(test_options_0, test_options_1);
  test_options_0.limit = 42;
  EXPECT_NE(test_options_0, test_options_1);
  test_options_1.limit = 42;
  EXPECT_EQ(test_options_0, test_options_1);
  test_options_1 = test_options_0;
  EXPECT_EQ(test_options_0, test_options_1);
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
