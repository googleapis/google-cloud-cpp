// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/internal/prefix_range_end.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
namespace {

TEST(PrefixRangeEndTest, Simple) {
  // This test assumes ASCII.
  EXPECT_EQ("foo0", PrefixRangeEnd("foo/"));
  EXPECT_EQ("fop", PrefixRangeEnd("foo"));
}

TEST(PrefixRangeEndTest, AllFFs) {
  char const* all_ff = "\xFF\xFF\xFF";
  auto actual = PrefixRangeEnd(std::string(all_ff, 3));
  EXPECT_EQ("", actual);
}

TEST(PrefixRangeEndTest, MostlyFFs) {
  char const* mostly_ff = "\xA0\xFF\xFF";
  char const* expected = "\xA1\x00\x00";
  auto actual = PrefixRangeEnd(std::string(mostly_ff, 3));
  EXPECT_EQ(std::string(expected, 3), actual);
}

}  // namespace
}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
