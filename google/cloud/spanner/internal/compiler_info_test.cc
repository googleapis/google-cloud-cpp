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

#include "google/cloud/spanner/internal/compiler_info.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

using ::testing::AnyOf;
using ::testing::ContainsRegex;
using ::testing::Eq;
using ::testing::MatchesRegex;

TEST(CompilerInfo, CompilerId) {
  auto cn = CompilerId();
  EXPECT_FALSE(cn.empty());
#ifndef _WIN32  // gMock's regex brackets don't work on Windows.
  EXPECT_THAT(cn, ContainsRegex(R"([A-Za-z]+)"));
#endif
}

TEST(CompilerInfo, CompilerVersion) {
  auto cv = CompilerVersion();
  EXPECT_FALSE(cv.empty());
#ifndef _WIN32  // gMock's regex brackets don't work on Windows.
  // Look for something that looks vaguely like an X.Y version number.
  EXPECT_THAT(cv, ContainsRegex(R"([0-9]+.[0-9]+)"));
#endif
}

TEST(CompilerInfo, CompilerFeatures) {
  auto cf = CompilerFeatures();
  EXPECT_FALSE(cf.empty());
  EXPECT_THAT(cf, AnyOf(Eq("noex"), Eq("ex")));
}

TEST(CompilerInfo, LanguageVersion) {
  auto lv = LanguageVersion();
  EXPECT_FALSE(lv.empty());
#ifndef _WIN32  // gMock's regex brackets don't work on Windows.
  EXPECT_THAT(lv, MatchesRegex(R"([0-9]+)"));
#endif
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
