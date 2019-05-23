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

#include "google/cloud/internal/build_info.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::testing::HasSubstr;
using ::testing::MatchesRegex;

TEST(BuildInfo, LanguageVersion) {
  auto lv = language_version();
  EXPECT_THAT(lv, ::testing::AnyOf(HasSubstr("-noex-"), HasSubstr("-ex-")));
#ifndef _WIN32
  EXPECT_THAT(lv, MatchesRegex(R"([0-9A-Za-z_.-]+)"));
#else
  // Brackets for regex above don't work on windows.
  EXPECT_THAT(lv, Not(HasSubstr(" ")));
#endif
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
