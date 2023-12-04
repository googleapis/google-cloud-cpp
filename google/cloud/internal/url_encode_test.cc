// Copyright 2023 Google LLC
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

#include "google/cloud/internal/url_encode.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

TEST(UrlEncode, Simple) {
  auto const* unencoded_string = "projects/*/resource/*";

  auto result = UrlEncode(unencoded_string);

  auto const* encoded_string = "projects%2F*%2Fresource%2F*";
  EXPECT_THAT(result, encoded_string);
}

TEST(UrlEncode, MultipleReplacements) {
  auto const* unencoded_string = R"( "#$%&+,/:;<)"
                                 R"(=>?@[\]^`{|})"
                                 "\177abcdABCD123";

  auto result = UrlEncode(unencoded_string);

  auto const* encoded_string =
      "%20%22%23%24%25%26%2B%2C%2F%3A%3B%3C"
      "%3D%3E%3F%40%5B%5C%5D%5E%60%7B%7C%7D"
      "%7FabcdABCD123";
  EXPECT_THAT(result, encoded_string);
}

TEST(UrlEncode, NotStdIsPrint) {
  auto const* unencoded_string = "\t";

  auto result = UrlEncode(unencoded_string);

  auto const* encoded_string = "%09";
  EXPECT_THAT(result, encoded_string);
}

TEST(UrlDecode, Simple) {
  auto const* encoded_string = "projects%2F*%2Fresource%2F*";

  auto result = UrlDecode(encoded_string);

  auto const* unencoded_string = "projects/*/resource/*";
  EXPECT_THAT(result, unencoded_string);
}

TEST(UrlDecode, MultipleReplacements) {
  auto const* encoded_string =
      "%20%22%23%24%25%26%2B%2C%2F%3A%3B%3C"
      "%3D%3E%3F%40%5B%5C%5D%5E%60%7B%7C%7D"
      "%7FabcdABCD123";

  auto result = UrlDecode(encoded_string);

  auto const* unencoded_string = R"( "#$%&+,/:;<)"
                                 R"(=>?@[\]^`{|})"
                                 "\177abcdABCD123";
  EXPECT_THAT(result, unencoded_string);
}

TEST(UrlDecode, PercentNoOverlap) {
  EXPECT_THAT(UrlEncode("%25"), "%2525");
  EXPECT_THAT(UrlDecode("%2525"), "%25");
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
