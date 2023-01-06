// Copyright 2022 Google LLC
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

#include "google/cloud/internal/debug_string.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

TEST(DebugString, TruncateString) {
  TracingOptions tracing_options;
  tracing_options.SetOptions("truncate_string_field_longer_than=8");
  struct Case {
    std::string actual;
    std::string expected;
  } cases[] = {
      {"1234567", "1234567"},
      {"12345678", "12345678"},
      {"123456789", "12345678...<truncated>..."},
      {"1234567890", "12345678...<truncated>..."},
  };
  for (auto const& c : cases) {
    EXPECT_EQ(c.expected, DebugString(c.actual, tracing_options));
  }
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
