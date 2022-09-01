// Copyright 2021 Google LLC
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

#include "google/cloud/internal/curl_wrappers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(CurlWrappers, VersionToCurlCode) {
  struct Test {
    std::string version;
    std::int64_t expected;
  } cases[] = {
    {"", CURL_HTTP_VERSION_NONE},
    {"default", CURL_HTTP_VERSION_NONE},
    {"1.0", CURL_HTTP_VERSION_1_0},
    {"1.1", CURL_HTTP_VERSION_1_1},
#if CURL_AT_LEAST_VERSION(7, 33, 0)
    {"2.0", CURL_HTTP_VERSION_2_0},
#endif  // CURL >= 7.33.0
#if CURL_AT_LEAST_VERSION(7, 47, 0)
    {"2TLS", CURL_HTTP_VERSION_2TLS},
#endif  // CURL >= 7.47.0
#if CURL_AT_LEAST_VERSION(7, 66, 0)
    {"3", CURL_HTTP_VERSION_3},
#endif  // CURL >= 7.66.0
  };
  for (auto const& test : cases) {
    SCOPED_TRACE("Testing with <" + test.version + ">");
    EXPECT_EQ(test.expected, VersionToCurlCode(test.version));
  }
}

TEST(CurlWrappers, DebugSendHeader) {
  struct TestCasse {
    std::string input;
    std::string expected;
  } cases[] = {
      {R"""(header1: v1)""", R"""(>> curl(Send Header): header1: v1)"""},
      {R"""(header1: value1
header2: value2)""",
       R"""(>> curl(Send Header): header1: value1
header2: value2)"""},
      {R"""(header1: value1
authorization: Bearer 1234567890
header2: value2)""",
       R"""(>> curl(Send Header): header1: value1
authorization: Bearer 1234567890
header2: value2)"""},
      {R"""(header1: value1
authorization: Bearer a1234567890.b1234567890.c1234567890.d1234567890
header2: value2)""",
       R"""(>> curl(Send Header): header1: value1
authorization: Bearer a1234567890.b1234567890.c1234567...<truncated>...
header2: value2)"""},
      {R"""(header1: value1
authorization: Bearer a1234567890.b1234567890.c1234567890.d1234567890)""",
       R"""(>> curl(Send Header): header1: value1
authorization: Bearer a1234567890.b1234567890.c1234567...<truncated>...)"""},
  };

  for (auto const& test : cases) {
    EXPECT_EQ(test.expected,
              DebugSendHeader(test.input.data(), test.input.size()));
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
