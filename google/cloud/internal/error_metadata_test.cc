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

#include "google/cloud/internal/error_metadata.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::HasSubstr;
using ::testing::Pair;
using ::testing::StartsWith;
using ::testing::UnorderedElementsAre;

TEST(ErrorContext, Basic) {
  auto const other =
      ErrorContext{{{"key", "value"}, {"filename", "the-filename"}}};
  auto actual = ErrorContext{other};
  EXPECT_EQ(other, actual);
  actual.push_back(std::make_pair("k0", "v0"));
  actual.emplace_back("k1", "v1");
  EXPECT_NE(other, actual);
  EXPECT_THAT(actual, UnorderedElementsAre(Pair("key", "value"),
                                           Pair("filename", "the-filename"),
                                           Pair("k0", "v0"), Pair("k1", "v1")));
}

TEST(ErrorContext, FormatEmpty) {
  EXPECT_EQ("error message", Format("error message", ErrorContext{}));
}

TEST(ErrorContext, Format) {
  auto const actual =
      Format("error message",
             ErrorContext{{{"key", "value"}, {"filename", "the-filename"}}});
  EXPECT_THAT(actual, StartsWith("error message"));
  EXPECT_THAT(actual, HasSubstr("key=value"));
  EXPECT_THAT(actual, HasSubstr("filename=the-filename"));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
