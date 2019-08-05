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

#include "google/cloud/internal/compiler_info.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

TEST(CompilerInfo, CompilerId) {
  auto cn = CompilerId();
  EXPECT_FALSE(cn.empty());
#ifndef _WIN32  // gMock's regex brackets don't work on Windows.
  EXPECT_THAT(cn, ::testing::ContainsRegex(R"([A-Za-z]+)"));
#endif
}

TEST(CompilerInfo, CompilerVersion) {
  auto cv = CompilerVersion();
  EXPECT_FALSE(cv.empty());
#ifndef _WIN32  // gMock's regex brackets don't work on Windows.
  // Look for something that looks vaguely like an X.Y version number.
  EXPECT_THAT(cv, ::testing::ContainsRegex(R"([0-9]+.[0-9]+)"));
#endif
}

TEST(CompilerInfo, CompilerFeatures) {
  using ::testing::Eq;
  auto cf = CompilerFeatures();
  EXPECT_FALSE(cf.empty());
  EXPECT_THAT(cf, ::testing::AnyOf(Eq("noex"), Eq("ex")));
}

TEST(CompilerInfo, LanguageVersion) {
  using ::testing::HasSubstr;
  auto lv = LanguageVersion();
  EXPECT_FALSE(lv.empty());
#ifndef _WIN32  // gMock's regex brackets don't work on Windows.
  EXPECT_THAT(lv, ::testing::MatchesRegex(R"([0-9]+)"));
#endif
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
