// Copyright 2018 Google LLC
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

#include "google/cloud/internal/binary_data_as_debug_string.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(BinaryDataAsDebugStringTest, Simple) {
  struct TestCase {
    std::string input;
    std::size_t max;
    std::string expected;
  } cases[] = {
      {"123abc", 0, "123abc"},
      {"234abc", 3, "234...<truncated>..."},
      {"3\n4\n5abc", 0, "3.4.5abc"},
      {"3\n4\n5a\n\n\nbc", 5, "3.4.5...<truncated>..."},
  };
  for (auto const& t : cases) {
    EXPECT_EQ(t.expected,
              BinaryDataAsDebugString(t.input.data(), t.input.size(), t.max));
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
