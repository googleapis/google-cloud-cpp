// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/opentelemetry/internal/recordable.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::IsEmpty;

auto constexpr kProjectId = "test-project";

TEST(SetTruncatableString, LessThanLimit) {
  google::devtools::cloudtrace::v2::TruncatableString proto;
  SetTruncatableString(proto, "value", 1000);
  EXPECT_EQ(proto.value(), "value");
  EXPECT_EQ(proto.truncated_byte_count(), 0);
}

TEST(SetTruncatableString, OverTheLimit) {
  google::devtools::cloudtrace::v2::TruncatableString proto;
  SetTruncatableString(proto, "abcde", 3);
  EXPECT_EQ(proto.value(), "abc");
  EXPECT_EQ(proto.truncated_byte_count(), 2);
}

TEST(SetTruncatableString, RespectsUnicodeSymbolBoundaries) {
  google::devtools::cloudtrace::v2::TruncatableString proto;
  // A UTF-8 encoded character that is 2 bytes wide.
  std::string const u2 = "\xd0\xb4";
  // The string `u2 + u2` is 4 bytes long. Truncation should respect the symbol
  // boundaries. i.e. we should not cut the symbol in half.
  SetTruncatableString(proto, u2 + u2, 3);
  EXPECT_EQ(proto.value(), u2);
  EXPECT_EQ(proto.truncated_byte_count(), 2);

  // A UTF-8 encoded character that is 3 bytes wide.
  std::string const u3 = "\xe6\x96\xad";
  SetTruncatableString(proto, u3 + u3, 5);
  EXPECT_EQ(proto.value(), u3);
  EXPECT_EQ(proto.truncated_byte_count(), 3);

  // Test the empty case.
  SetTruncatableString(proto, u3 + u3, 2);
  EXPECT_THAT(proto.value(), IsEmpty());
  EXPECT_EQ(proto.truncated_byte_count(), 6);
}

TEST(Recordable, Compiles) {
  auto rec = Recordable(Project(kProjectId));
  GTEST_SUCCEED();
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
