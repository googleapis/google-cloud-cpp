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

#include "google/cloud/internal/base64_transforms.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::ContainsRegex;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Not;

TEST(Base64, RoundTrip) {
  std::vector<std::pair<std::string, std::string>> test_cases = {
      {"abcd", "YWJjZA=="},
      {"abcde", "YWJjZGU="},
      {"abcdef", "YWJjZGVm"},
      {"abcdefg", "YWJjZGVmZw=="},
      {"abcdefgh", "YWJjZGVmZ2g="},
      {"abcdefghi", "YWJjZGVmZ2hp"},
      {"abcdefghij", "YWJjZGVmZ2hpag=="},
      {"abcdefghijk", "YWJjZGVmZ2hpams="},
      {"abcdefghijkl", "YWJjZGVmZ2hpamts"},
      {"abcdefghijklm", "YWJjZGVmZ2hpamtsbQ=="},
      {"abcdefghijklmn", "YWJjZGVmZ2hpamtsbW4="},
      {"abcdefghijklmno", "YWJjZGVmZ2hpamtsbW5v"},
      {"abcdefghijklmnop", "YWJjZGVmZ2hpamtsbW5vcA=="},
      {"abcdefghijklmnopq", "YWJjZGVmZ2hpamtsbW5vcHE="},
      {"abcdefghijklmnopqr", "YWJjZGVmZ2hpamtsbW5vcHFy"},
      {"abcdefghijklmnopqrs", "YWJjZGVmZ2hpamtsbW5vcHFycw=="},
      {"abcdefghijklmnopqrst", "YWJjZGVmZ2hpamtsbW5vcHFyc3Q="},
      {"abcdefghijklmnopqrstu", "YWJjZGVmZ2hpamtsbW5vcHFyc3R1"},
      {"abcdefghijklmnopqrstuv", "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dg=="},
      {"abcdefghijklmnopqrstuvw", "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnc="},
      {"abcdefghijklmnopqrstuvwx", "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4"},
      {"abcdefghijklmnopqrstuvwxy", "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eQ=="},
      {"abcdefghijklmnopqrstuvwxyz", "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXo="},
  };
  for (auto const& test_case : test_cases) {
    Base64Encoder enc;
    for (auto c : test_case.first) enc.PushBack(c);
    auto const encoded = std::move(enc).FlushAndPad();
    EXPECT_EQ(test_case.second, encoded);
    EXPECT_STATUS_OK(ValidateBase64String(encoded)) << test_case.first;
    Base64Decoder dec(encoded);
    auto const decoded = std::string{dec.begin(), dec.end()};
    EXPECT_EQ(test_case.first, decoded);
  }
}

TEST(Base64, RFC4648TestVectors) {
  // https://tools.ietf.org/html/rfc4648#section-10
  std::vector<std::pair<std::string, std::string>> test_cases = {
      {"", ""},
      {"a", "YQ=="},
      {"ab", "YWI="},
      {"abc", "YWJj"},
      {"abcd", "YWJjZA=="},
      {"abcde", "YWJjZGU="},
      {"abcdef", "YWJjZGVm"},
  };
  for (auto const& test_case : test_cases) {
    Base64Encoder enc;
    for (auto c : test_case.first) enc.PushBack(c);
    auto const encoded = std::move(enc).FlushAndPad();
    EXPECT_EQ(test_case.second, encoded);
    EXPECT_STATUS_OK(ValidateBase64String(encoded)) << test_case.first;
    Base64Decoder dec(encoded);
    auto const decoded = std::string{dec.begin(), dec.end()};
    EXPECT_EQ(test_case.first, decoded);
  }
}

TEST(Base64, WikiExample) {
  // https://en.wikipedia.org/wiki/Base64#Examples
  std::string const plain =
      "Man is distinguished, not only by his reason, but by this singular "
      "passion from other animals, which is a lust of the mind, that by a "
      "perseverance of delight in the continued and indefatigable generation "
      "of knowledge, exceeds the short vehemence of any carnal pleasure.";
  std::string const expected =
      "TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0"
      "aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1"
      "c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0"
      "aGUgY29udGludWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdl"
      "LCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4"
      "=";

  Base64Encoder enc;
  for (auto c : plain) enc.PushBack(c);
  auto const actual = std::move(enc).FlushAndPad();
  EXPECT_EQ(actual, expected);
  EXPECT_STATUS_OK(ValidateBase64String(actual));
  Base64Decoder dec(actual);
  auto const decoded = std::string{dec.begin(), dec.end()};
  EXPECT_EQ(plain, decoded);
}

TEST(Base64, ValidateBase64StringFailures) {
  // Bad lengths.
  for (std::string const base64 : {"x", "xx", "xxx"}) {
    auto status = ValidateBase64String(base64);
    EXPECT_THAT(status, StatusIs(Not(StatusCode::kOk),
                                 ContainsRegex("Invalid base64.*at offset 0")));
  }

  for (std::string const base64 : {"xxxxx", "xxxxxx", "xxxxxxx"}) {
    auto status = ValidateBase64String(base64);
    EXPECT_THAT(status, StatusIs(Not(StatusCode::kOk),
                                 ContainsRegex("Invalid base64.*at offset 4")));
  }

  // Chars outside base64 alphabet.
  for (std::string const base64 : {".xxx", "x.xx", "xx.x", "xxx.", "xx.="}) {
    auto status = ValidateBase64String(base64);
    EXPECT_THAT(status, StatusIs(Not(StatusCode::kOk),
                                 ContainsRegex("Invalid base64.*at offset 0")));
  }

  // Non-zero padding bits.
  for (std::string const base64 : {"xx==", "xxx="}) {
    auto status = ValidateBase64String(base64);
    EXPECT_THAT(status, StatusIs(Not(StatusCode::kOk),
                                 ContainsRegex("Invalid base64.*at offset 0")));
  }
}

TEST(Base64, UrlsafeBase64Encode) {
  // Produced input using:
  //     echo 'TG9yZ+W0gaXBz/dW1cMACg==' | openssl base64 -d | od -t x1
  std::vector<std::uint8_t> input{
      0x4c, 0x6f, 0x72, 0x67, 0xe5, 0xb4, 0x81, 0xa5,
      0xc1, 0xcf, 0xf7, 0x56, 0xd5, 0xc3, 0x00, 0x0a,
  };
  EXPECT_EQ("TG9yZ-W0gaXBz_dW1cMACg", UrlsafeBase64Encode(input));
}

TEST(Base64, Base64Decode) {
  // Produced input using:
  //     echo 'TG9yZ+W0gaXBz/dW1cMACg==' | openssl base64 -d | od -t x1
  std::vector<std::uint8_t> expected{
      0x4c, 0x6f, 0x72, 0x67, 0xe5, 0xb4, 0x81, 0xa5,
      0xc1, 0xcf, 0xf7, 0x56, 0xd5, 0xc3, 0x00, 0x0a,
  };
  EXPECT_THAT(UrlsafeBase64Decode("TG9yZ-W0gaXBz_dW1cMACg").value(),
              ElementsAreArray(expected));
}

TEST(Base64, Base64DecodePadding) {
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

  EXPECT_THAT(UrlsafeBase64Decode("QQ").value(), ElementsAre('A'));
  EXPECT_THAT(UrlsafeBase64Decode("QUI").value(), ElementsAre('A', 'B'));
  EXPECT_THAT(UrlsafeBase64Decode("QUJD").value(), ElementsAre('A', 'B', 'C'));
  EXPECT_THAT(UrlsafeBase64Decode("QUJDRA").value(),
              ElementsAre('A', 'B', 'C', 'D'));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
