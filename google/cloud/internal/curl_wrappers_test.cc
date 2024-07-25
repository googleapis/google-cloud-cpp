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
#include "google/cloud/internal/curl_options.h"
#include "absl/strings/string_view.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(CurlWrappers, VersionToCurlCode) {
  struct Test {
    std::string version;
    std::int64_t expected;
  } cases[] = {
      {"", CURL_HTTP_VERSION_NONE},     {"default", CURL_HTTP_VERSION_NONE},
      {"1.0", CURL_HTTP_VERSION_1_0},   {"1.1", CURL_HTTP_VERSION_1_1},
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
      {R"""(header1: no-marker-no-nl)""",
       R"""(>> curl(Send Header): header1: no-marker-no-nl)"""},
      {R"""(header1: no-marker-w-nl
)""",
       R"""(>> curl(Send Header): header1: no-marker-w-nl
)"""},
      {R"""(header1: no-marker-w-nl-and-data
header2: value2
)""",
       R"""(>> curl(Send Header): header1: no-marker-w-nl-and-data
header2: value2
)"""},

      {R"""(header1: short-no-nl
authorization: Bearer 012345678901234567890123456789)""",
       R"""(>> curl(Send Header): header1: short-no-nl
authorization: Bearer 012345678901234567890123456789)"""},
      {R"""(header1: short-w-nl
authorization: Bearer 012345678901234567890123456789
)""",
       R"""(>> curl(Send Header): header1: short-w-nl
authorization: Bearer 012345678901234567890123456789
)"""},
      {R"""(header1: short-w-nl-and-data
authorization: Bearer 012345678901234567890123456789
header2: value2
)""",
       R"""(>> curl(Send Header): header1: short-w-nl-and-data
authorization: Bearer 012345678901234567890123456789
header2: value2
)"""},

      {R"""(header1: exact-no-nl
authorization: Bearer 01234567890123456789012345678912)""",
       R"""(>> curl(Send Header): header1: exact-no-nl
authorization: Bearer 01234567890123456789012345678912)"""},
      {R"""(header1: exact-w-nl
authorization: Bearer 01234567890123456789012345678912
)""",
       R"""(>> curl(Send Header): header1: exact-w-nl
authorization: Bearer 01234567890123456789012345678912
)"""},
      {R"""(header1: exact-w-nl-and-data
authorization: Bearer 01234567890123456789012345678912
header2: value2
)""",
       R"""(>> curl(Send Header): header1: exact-w-nl-and-data
authorization: Bearer 01234567890123456789012345678912
header2: value2
)"""},

      {R"""(header1: long-no-nl
authorization: Bearer 012345678901234567890123456789123456)""",
       R"""(>> curl(Send Header): header1: long-no-nl
authorization: Bearer 01234567890123456789012345678912...<truncated>...)"""},
      {R"""(header1: long-w-nl
authorization: Bearer 012345678901234567890123456789123456
)""",
       R"""(>> curl(Send Header): header1: long-w-nl
authorization: Bearer 01234567890123456789012345678912...<truncated>...
)"""},
      {R"""(header1: long-w-nl-and-data
authorization: Bearer 012345678901234567890123456789123456
header2: value2
)""",
       R"""(>> curl(Send Header): header1: long-w-nl-and-data
authorization: Bearer 01234567890123456789012345678912...<truncated>...
header2: value2
)"""},
  };

  for (auto const& test : cases) {
    EXPECT_EQ(test.expected,
              DebugSendHeader(test.input.data(), test.input.size()));
  }
}

TEST(CurlWrappers, DebugInfo) {
  struct TestCasse {
    std::string input;
    std::string expected;
  } cases[] = {
      {R"""(no-marker-no-nl)""", R"""(== curl(Info): no-marker-no-nl)"""},
      {R"""(no-marker-w-nl
)""",
       R"""(== curl(Info): no-marker-w-nl
)"""},

      {R"""([HTTP/2] [1] [authorization: Bearer 012345678901234567890123456789)""",
       R"""(== curl(Info): [HTTP/2] [1] [authorization: Bearer 012345678901234567890123456789)"""},
      {R"""([HTTP/2] [1] [authorization: Bearer 01234567890123456789012345678912)""",
       R"""(== curl(Info): [HTTP/2] [1] [authorization: Bearer 01234567890123456789012345678912)"""},

      {R"""([HTTP/2] [1] [authorization: Bearer 012345678901234567890123456789123456)""",
       R"""(== curl(Info): [HTTP/2] [1] [authorization: Bearer 01234567890123456789012345678912...<truncated>...)"""},
  };

  for (auto const& test : cases) {
    EXPECT_EQ(test.expected, DebugInfo(test.input.data(), test.input.size()));
  }
}

TEST(CurlWrappers, CurlInitializeOptions) {
  auto defaults = CurlInitializeOptions({});
  EXPECT_TRUE(defaults.get<EnableCurlSslLockingOption>());
  EXPECT_TRUE(defaults.get<EnableCurlSigpipeHandlerOption>());

  auto override1 =
      CurlInitializeOptions(Options{}.set<EnableCurlSslLockingOption>(false));
  EXPECT_FALSE(override1.get<EnableCurlSslLockingOption>());
  EXPECT_TRUE(override1.get<EnableCurlSigpipeHandlerOption>());

  auto override2 = CurlInitializeOptions(
      Options{}.set<EnableCurlSigpipeHandlerOption>(false));
  EXPECT_TRUE(override2.get<EnableCurlSslLockingOption>());
  EXPECT_FALSE(override2.get<EnableCurlSigpipeHandlerOption>());
}

TEST(CurlWrappers, AppendHeaderData) {
  absl::string_view inputs[] = {
      "HTTP/1.1 200 OK\r\n",
      "X-Test1: value1\r\n",
      "X-Test1: value2\r\n",
      "X-Test2: value",
      "X-Test2: a:b:c\r\n",
      "X-Test3:v3\r\n",
      "X-Test3:    v4\r\n",
      "",
      "\r",
      "\n",
  };
  CurlReceivedHeaders headers;
  for (auto v : inputs) CurlAppendHeaderData(headers, v.data(), v.size());
  EXPECT_THAT(headers,
              UnorderedElementsAre(
                  Pair("http/1.1 200 ok", ""), Pair("x-test1", "value1"),
                  Pair("x-test1", "value2"), Pair("x-test2", "a:b:c"),
                  Pair("x-test3", "v3"), Pair("x-test3", "v4")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
