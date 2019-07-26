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

#include "google/cloud/spanner/internal/base64.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>
#include <limits>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::testing::HasSubstr;

TEST(Base64, RoundTrip) {
  char c = std::numeric_limits<char>::min();
  std::string chars(1, c);
  while (c != std::numeric_limits<char>::max()) {
    chars.push_back(++c);
  }

  // Empty string.
  std::string bytes;
  auto const encoded = internal::Base64Encode(bytes);
  EXPECT_EQ(0, encoded.size());
  auto const decoded = internal::Base64Decode(encoded);
  EXPECT_STATUS_OK(decoded) << encoded;
  if (decoded) {
    EXPECT_EQ(bytes, *decoded);
  }

  // All 1-char strings.
  bytes.resize(1);
  for (auto c : chars) {
    bytes[0] = c;
    auto const encoded = internal::Base64Encode(bytes);
    EXPECT_EQ(4, encoded.size()) << bytes;
    auto const decoded = internal::Base64Decode(encoded);
    EXPECT_STATUS_OK(decoded) << encoded;
    if (decoded) {
      EXPECT_EQ(bytes, *decoded) << encoded;
    }
  }

  // All 2-char strings.
  bytes.resize(2);
  for (auto c0 : chars) {
    bytes[0] = c0;
    for (auto c1 : chars) {
      bytes[1] = c1;
      auto const encoded = internal::Base64Encode(bytes);
      EXPECT_EQ(4, encoded.size()) << bytes;
      auto const decoded = internal::Base64Decode(encoded);
      EXPECT_STATUS_OK(decoded) << encoded;
      if (decoded) {
        EXPECT_EQ(bytes, *decoded) << encoded;
      }
    }
  }

  // Some 3-char strings.
  bytes.resize(3);
  for (auto c0 : {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j'}) {
    bytes[0] = c0;
    for (auto c1 : chars) {
      bytes[1] = c1;
      for (auto c2 : chars) {
        bytes[2] = c2;
        auto const encoded = internal::Base64Encode(bytes);
        EXPECT_EQ(4, encoded.size()) << bytes;
        auto const decoded = internal::Base64Decode(encoded);
        EXPECT_STATUS_OK(decoded) << encoded;
        if (decoded) {
          EXPECT_EQ(bytes, *decoded) << encoded;
        }
      }
    }
  }
}

TEST(Base64, LongerRoundTrip) {
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
    EXPECT_EQ(test_case.second, internal::Base64Encode(test_case.first));
    auto const decoded = internal::Base64Decode(test_case.second);
    EXPECT_STATUS_OK(decoded) << test_case.second;
    if (decoded) {
      EXPECT_EQ(test_case.first, *decoded);
    }
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
    EXPECT_EQ(test_case.second, internal::Base64Encode(test_case.first));
    auto const decoded = internal::Base64Decode(test_case.second);
    EXPECT_STATUS_OK(decoded) << test_case.second;
    if (decoded) {
      EXPECT_EQ(test_case.first, *decoded);
    }
  }
}

TEST(Base64, WikiExample) {
  // https://en.wikipedia.org/wiki/Base64#Examples
  std::string const plain =
      "Man is distinguished, not only by his reason, but by this singular "
      "passion from other animals, which is a lust of the mind, that by a "
      "perseverance of delight in the continued and indefatigable generation "
      "of knowledge, exceeds the short vehemence of any carnal pleasure.";
  std::string const coded =
      "TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0"
      "aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1"
      "c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0"
      "aGUgY29udGludWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdl"
      "LCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4"
      "=";
  EXPECT_EQ(coded, internal::Base64Encode(plain));
  auto const decoded = internal::Base64Decode(coded);
  EXPECT_STATUS_OK(decoded) << coded;
  if (decoded) {
    EXPECT_EQ(plain, *decoded);
  }
}

TEST(Base64, DecodeFailures) {
  // Bad lengths.
  for (std::string const base64 : {"x", "xx", "xxx"}) {
    auto decoded = internal::Base64Decode(base64);
    EXPECT_FALSE(decoded.ok());
    if (!decoded) {
      EXPECT_THAT(decoded.status().message(), HasSubstr("Invalid base64"));
      EXPECT_THAT(decoded.status().message(), HasSubstr("at offset 0"));
    }
  }
  for (std::string const base64 : {"xxxxx", "xxxxxx", "xxxxxxx"}) {
    auto decoded = internal::Base64Decode(base64);
    EXPECT_FALSE(decoded.ok());
    if (!decoded) {
      EXPECT_THAT(decoded.status().message(), HasSubstr("Invalid base64"));
      EXPECT_THAT(decoded.status().message(), HasSubstr("at offset 4"));
    }
  }

  // Chars outside base64 alphabet.
  for (std::string const base64 : {".xxx", "x.xx", "xx.x", "xxx.", "xx.="}) {
    auto decoded = internal::Base64Decode(base64);
    EXPECT_FALSE(decoded.ok());
    if (!decoded) {
      EXPECT_THAT(decoded.status().message(), HasSubstr("Invalid base64"));
      EXPECT_THAT(decoded.status().message(), HasSubstr("at offset 0"));
    }
  }

  // Non-zero padding bits.
  for (std::string const base64 : {"xx==", "xxx="}) {
    auto decoded = internal::Base64Decode(base64);
    EXPECT_FALSE(decoded.ok());
    if (!decoded) {
      EXPECT_THAT(decoded.status().message(), HasSubstr("Invalid base64"));
      EXPECT_THAT(decoded.status().message(), HasSubstr("at offset 0"));
    }
  }
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
