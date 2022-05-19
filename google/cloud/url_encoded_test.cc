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

#include "google/cloud/url_encoded.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::IsEmpty;

TEST(UrlEncodeString, AllPrintableAsciiCharacters) {
  std::string escaped_printable_ascii_chars =
      R"(%20%21%22%23%24%25%26%27%28%29%2A%2B%2C-.%2F0123456789%3A%3B%3C%3D%3E%3F%40ABCDEFGHIJKLMNOPQRSTUVWXYZ%5B%5C%5D%5E_%60abcdefghijklmnopqrstuvwxyz%7B%7C%7D~)";
  std::string printable_ascii_chars =
      R"( !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~)";
  auto result = UrlEncode(printable_ascii_chars);
  EXPECT_EQ(result, escaped_printable_ascii_chars);
}

TEST(UrlEncodeString, EmptyString) {
  auto result = UrlEncode({});
  EXPECT_THAT(result, IsEmpty());
}

TEST(UrlEncodeString, OneUnreservedCharacter) {
  auto result = UrlEncode("T");
  EXPECT_EQ(result, "T");
}

TEST(UrlEncodeString, OneReservedCharacter) {
  auto result = UrlEncode("%");
  EXPECT_EQ(result, "%25");
}

TEST(UrlDecodeString, AllPrintableAsciiCharacters) {
  std::string escaped_printable_ascii_chars =
      R"(%20%21%22%23%24%25%26%27%28%29%2A%2B%2C-.%2F0123456789%3A%3B%3C%3D%3E%3F%40ABCDEFGHIJKLMNOPQRSTUVWXYZ%5B%5C%5D%5E_%60abcdefghijklmnopqrstuvwxyz%7B%7C%7D~)";
  std::string printable_ascii_chars =
      R"( !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~)";
  auto result = UrlDecode(escaped_printable_ascii_chars);
  EXPECT_EQ(result, printable_ascii_chars);
}

TEST(UrlDecodeString, EmptyString) {
  auto result = UrlDecode({});
  EXPECT_THAT(result, IsEmpty());
}

TEST(UrlDecodeString, OneUnreservedCharacter) {
  auto result = UrlDecode("T");
  EXPECT_EQ(result, "T");
}

TEST(UrlDecodeString, OneReservedCharacter) {
  auto result = UrlDecode("%25");
  EXPECT_EQ(result, "%");
}

TEST(UrlEncoding, RoundTrip) {
  std::string printable_ascii_chars =
      R"( !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~)";
  auto result = UrlDecode(UrlEncode(printable_ascii_chars));
  EXPECT_EQ(printable_ascii_chars, result);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
