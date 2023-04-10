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
#include <gtest/gtest.h>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

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
  // The unicode symbol "д" is 2 bytes wide. So the string "дд" is 4 bytes long.
  // Truncation should respect the symbol boundaries. i.e. We should not cut the
  // symbol in half.
  SetTruncatableString(proto, "дд", 3);
  EXPECT_EQ(proto.value(), "д");
  EXPECT_EQ(proto.truncated_byte_count(), 2);

  // "断" is 3 bytes wide.
  SetTruncatableString(proto, "断断", 5);
  EXPECT_EQ(proto.value(), "断");
  EXPECT_EQ(proto.truncated_byte_count(), 3);
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
