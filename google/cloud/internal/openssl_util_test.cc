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

#include "google/cloud/internal/openssl_util.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

TEST(OpensslUtilTest, Base64Encode) {
  // Produced input using:
  //     echo 'TG9yZ+W0gaXBz/dW1cMACg==' | openssl base64 -d | od -t x1
  std::vector<std::uint8_t> input{
      0x4c, 0x6f, 0x72, 0x67, 0xe5, 0xb4, 0x81, 0xa5,
      0xc1, 0xcf, 0xf7, 0x56, 0xd5, 0xc3, 0x00, 0x0a,
  };
  EXPECT_EQ("TG9yZ+W0gaXBz/dW1cMACg==", Base64Encode(input));
  EXPECT_EQ("TG9yZ-W0gaXBz_dW1cMACg", UrlsafeBase64Encode(input));
}

TEST(OpensslUtilTest, Base64Decode) {
  // Produced input using:
  //     echo 'TG9yZ+W0gaXBz/dW1cMACg==' | openssl base64 -d | od -t x1
  std::vector<std::uint8_t> expected{
      0x4c, 0x6f, 0x72, 0x67, 0xe5, 0xb4, 0x81, 0xa5,
      0xc1, 0xcf, 0xf7, 0x56, 0xd5, 0xc3, 0x00, 0x0a,
  };
  EXPECT_THAT(Base64Decode("TG9yZ+W0gaXBz/dW1cMACg==").value(),
              ElementsAreArray(expected));
  EXPECT_THAT(UrlsafeBase64Decode("TG9yZ-W0gaXBz_dW1cMACg").value(),
              ElementsAreArray(expected));
}

TEST(OpensslUtilTest, Base64Failure) {
  EXPECT_THAT(Base64Decode("xx"), StatusIs(StatusCode::kInvalidArgument));
}

TEST(OpensslUtilTest, Base64DecodePadding) {
  // Produced input using:
  // cSpell:disable
  // $ echo -n 'A' | openssl base64 -e
  // QQ==
  // $ echo -n 'AB' | openssl base64 -e
  // QUI=
  // $ echo -n 'ABC' | openssl base64 -e
  // QUJD
  // $ echo -n 'ABCD' | openssl base64 -e
  // QUJDRAo=
  // cSpell:enable

  EXPECT_THAT(Base64Decode("QQ==").value(), ElementsAre('A'));
  EXPECT_THAT(UrlsafeBase64Decode("QQ").value(), ElementsAre('A'));
  EXPECT_THAT(Base64Decode("QUI=").value(), ElementsAre('A', 'B'));
  EXPECT_THAT(UrlsafeBase64Decode("QUI").value(), ElementsAre('A', 'B'));
  EXPECT_THAT(Base64Decode("QUJD").value(), ElementsAre('A', 'B', 'C'));
  EXPECT_THAT(UrlsafeBase64Decode("QUJD").value(), ElementsAre('A', 'B', 'C'));
  EXPECT_THAT(Base64Decode("QUJDRA==").value(),
              ElementsAre('A', 'B', 'C', 'D'));
  EXPECT_THAT(UrlsafeBase64Decode("QUJDRA").value(),
              ElementsAre('A', 'B', 'C', 'D'));
}

TEST(OpensslUtilTest, MD5HashEmpty) {
  auto const actual = MD5Hash("");
  // I used this command to get the expected value:
  // /bin/echo -n "" | openssl md5
  auto const expected =
      std::vector<std::uint8_t>{0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04,
                                0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e};
  EXPECT_THAT(actual, ElementsAreArray(expected));
}

TEST(OpensslUtilTest, MD5HashSimple) {
  auto const actual = MD5Hash("The quick brown fox jumps over the lazy dog");
  // I used this command to get the expected value:
  // /bin/echo -n "The quick brown fox jumps over the lazy dog" |
  //     openssl md5 -binary | openssl base64
  auto const expected =
      std::vector<std::uint8_t>{0x9e, 0x10, 0x7d, 0x9d, 0x37, 0x2b, 0xb6, 0x82,
                                0x6b, 0xd8, 0x1d, 0x35, 0x42, 0xa4, 0x19, 0xd6};
  EXPECT_THAT(actual, ElementsAreArray(expected));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
