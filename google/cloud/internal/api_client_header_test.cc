// Copyright 2019 Google LLC
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

#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/compiler_info.h"
#include "absl/strings/str_split.h"
#include <gmock/gmock.h>
#include <string>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::Matcher;
using ::testing::ResultOf;
using ::testing::StartsWith;
using ::testing::UnorderedElementsAre;

Matcher<std::string const&> IsCppIdentifier() {
  // This is a change detector test. A bit boring, but the point is that the
  // format of this field *must be just so*:
  return StartsWith("gl-cpp/" + CompilerId() + "-" + CompilerVersion() + "-" +
                    CompilerFeatures() + "-" + LanguageVersion());
}

Matcher<std::string const&> IsLibraryIdentifier(std::string const& prefix) {
  return AllOf(StartsWith(prefix), HasSubstr(version_string()));
}

Matcher<std::string const&> IsClientHeader(std::string const& prefix) {
  return ResultOf(
      "x-goog-api-client elements match C++ and Library version identifiers",
      [](std::string const& header) -> std::vector<std::string> {
        return absl::StrSplit(header, ' ');
      },
      UnorderedElementsAre(IsCppIdentifier(), IsLibraryIdentifier(prefix)));
}

TEST(ApiClientHeaderTest, HandCrafted) {
  auto const actual = HandCraftedLibClientHeader();
  EXPECT_THAT(actual, IsClientHeader("gccl/"));
}

TEST(ApiClientHeaderTest, Generated) {
  auto const actual = GeneratedLibClientHeader();
  EXPECT_THAT(actual, IsClientHeader("gapic/"));
  EXPECT_THAT(actual, HasSubstr("generated"));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
