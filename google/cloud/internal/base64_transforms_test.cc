// Copyright 2021 Google LLC
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

#include "google/cloud/internal/base64_transforms.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::ContainsRegex;
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
      {"f", "Zg=="},
      {"fo", "Zm8="},
      {"foo", "Zm9v"},
      {"foob", "Zm9vYg=="},
      {"fooba", "Zm9vYmE="},
      {"foobar", "Zm9vYmFy"},
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

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
