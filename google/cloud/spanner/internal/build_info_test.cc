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

#include "google/cloud/spanner/internal/build_info.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

TEST(BuildInfo, CompilerId) {
  auto cn = CompilerId();
  EXPECT_FALSE(cn.empty());
  EXPECT_THAT(cn, ::testing::Not(::testing::HasSubstr("@")));
}

TEST(BuildInfo, CompilerVersion) {
  auto cv = CompilerVersion();
  EXPECT_FALSE(cv.empty());
  EXPECT_THAT(cv, ::testing::Not(::testing::HasSubstr("@")));
#ifndef _WIN32  // gMock's regex brackets don't work on Windows.
  // Look for something that looks vaguely like an X.Y version number.
  EXPECT_THAT(cv, ::testing::ContainsRegex(R"([0-9].[0-9])"));
#endif
}

TEST(BuildInfo, BuildFlags) {
  auto bf = BuildFlags();
  EXPECT_THAT(bf, ::testing::Not(::testing::HasSubstr("@")));
}

TEST(BuildInfo, CompilerFeatures) {
  using ::testing::Eq;
  auto cf = CompilerFeatures();
  EXPECT_FALSE(cf.empty());
  EXPECT_THAT(cf, ::testing::AnyOf(Eq("noex"), Eq("ex")));
}

TEST(BuildInfo, LanguageVersion) {
  using ::testing::HasSubstr;
  auto lv = LanguageVersion();
  EXPECT_FALSE(lv.empty());
  EXPECT_THAT(lv, ::testing::Not(::testing::HasSubstr("@")));
#ifndef _WIN32  // gMock's regex brackets don't work on Windows.
  EXPECT_THAT(lv, ::testing::MatchesRegex(R"([0-9A-Za-z_.-]+)"));
#endif
}

TEST(BuildInfo, IsRelease) {
  bool const b = IsRelease();
  // We want to test this, but either value is fine.
  EXPECT_TRUE(b || !b);
}

TEST(BuildInfo, BuildMetadata) {
  auto const md = BuildMetadata();
  EXPECT_FALSE(md.empty());
  EXPECT_THAT(md, ::testing::Not(::testing::HasSubstr("@")));
}

TEST(BuildInfo, ApiClientHeader) {
  // The build info is used to generate the "API Client Header", which is a
  // gRPC metadata attribute with the name 'x-goog-api-client'. This test
  // generates that whole string as a sanity check that it will contain the
  // desired format.

  std::string const api_client_header = "gl-cpp/" +                 //
                                        CompilerId() + '-' +        //
                                        CompilerVersion() + '-' +   //
                                        CompilerFeatures() + '-' +  //
                                        LanguageVersion();
  EXPECT_THAT(api_client_header, ::testing::Not(::testing::HasSubstr("@")));

#ifndef _WIN32  // gMock's regex brackets don't work on Windows.
  EXPECT_THAT(api_client_header,
              ::testing::MatchesRegex(
                  R"(gl-cpp/[A-Za-z0-9]+-[0-9.+-]+-(no)?ex-20[1-9][0-9])"));
#endif
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
