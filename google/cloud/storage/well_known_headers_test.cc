// Copyright 2018 Google LLC
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

#include "google/cloud/storage/well_known_headers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::testing::HasSubstr;

/// @test Verify that CustomHeader works as expected.
TEST(WellKnownHeader, CustomHeader) {
  CustomHeader header("x-goog-testbench-instructions", "do-stuff");
  std::ostringstream os;
  os << header;
  EXPECT_THAT(os.str(), HasSubstr("do-stuff"));
  EXPECT_THAT(os.str(), HasSubstr("x-goog-testbench-instructions"));
}

/// @test Verify that EncryptionKey streaming works as expected.
TEST(WellKnownHeader, EncryptionKey) {
  EncryptionKey header(
      EncryptionKeyData{"test-algo", "test-fake-key", "test-sha"});
  std::ostringstream os;
  os << header;
  auto actual = os.str();
  std::string prefix = "x-goog-encryption";
  EXPECT_THAT(actual, HasSubstr(prefix + "-algorithm: test-algo"));
  EXPECT_THAT(actual, HasSubstr(prefix + "-key: test-fake-key"));
  EXPECT_THAT(actual, HasSubstr(prefix + "-key-sha256: test-sha"));
}

/// @test Verify that EncryptionKey::FromBinaryKey works as expected.
TEST(WellKnownHeader, EncryptionKeyFromBinary) {
  std::string key("01234567");
  auto header = EncryptionKey::FromBinaryKey(key);
  ASSERT_TRUE(header.has_value());
  ASSERT_EQ("AES256", header.value().algorithm);
  // used:
  //   /bin/echo -n "01234567" | openssl base64
  // to get the key value.
  EXPECT_EQ("MDEyMzQ1Njc=", header.value().key);
  // used:
  //   /bin/echo -n "01234567" | sha256sum | awk '{printf("%s", $1);}' |
  //       xxd -r -p | openssl base64
  // to get the SHA256 value of the key.
  EXPECT_EQ("kkWSubED8U+DP6r7Z/SAaR8BmIqkV8AGF2n1jNRzEbw=",
            header.value().sha256);
}

/// @test Verify that EncryptionKey::FromBase64Key works as expected.
TEST(WellKnownHeader, EncryptionKeyFromBase64) {
  std::string key("0123456789-ABCDEFGHIJ-0123456789");
  auto expected = EncryptionKey::FromBinaryKey(key);
  ASSERT_TRUE(expected.has_value());
  // Generated with:
  //     /bin/echo -n 0123456789-ABCDEFGHIJ-0123456789 | openssl base64
  EXPECT_EQ("MDEyMzQ1Njc4OS1BQkNERUZHSElKLTAxMjM0NTY3ODk=",
            expected.value().key);
  auto actual = EncryptionKey::FromBase64Key(expected.value().key);
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(expected.value().algorithm, actual.value().algorithm);
  EXPECT_EQ(expected.value().key, actual.value().key);
  EXPECT_EQ(expected.value().sha256, actual.value().sha256);
}

/// @test Verify that SourceEncryptionKey streaming works as expected.
TEST(WellKnownHeader, SourceEncryptionKey) {
  SourceEncryptionKey header(
      EncryptionKeyData{"test-algo", "test-fake-key", "test-sha"});
  std::ostringstream os;
  os << header;
  auto actual = os.str();
  std::string prefix = "x-goog-copy-source-encryption";
  EXPECT_THAT(actual, HasSubstr(prefix + "-algorithm: test-algo"));
  EXPECT_THAT(actual, HasSubstr(prefix + "-key: test-fake-key"));
  EXPECT_THAT(actual, HasSubstr(prefix + "-key-sha256: test-sha"));
}

/// @test Verify that SourceEncryptionKey::FromBinaryKey works as expected.
TEST(WellKnownHeader, SourceEncryptionKeyFromBinary) {
  std::string key("01234567");
  auto header = SourceEncryptionKey::FromBinaryKey(key);
  ASSERT_TRUE(header.has_value());
  ASSERT_EQ("AES256", header.value().algorithm);
  // used:
  //   /bin/echo -n "01234567" | openssl base64
  // to get the key value.
  EXPECT_EQ("MDEyMzQ1Njc=", header.value().key);
  // used:
  //   /bin/echo -n "01234567" | sha256sum | awk '{printf("%s", $1);}' |
  //       xxd -r -p | openssl base64
  // to get the SHA256 value of the key.
  EXPECT_EQ("kkWSubED8U+DP6r7Z/SAaR8BmIqkV8AGF2n1jNRzEbw=",
            header.value().sha256);
}

/// @test Verify that SourceEncryptionKey::FromBase64Key works as expected.
TEST(WellKnownHeader, SourceEncryptionKeyFromBase64) {
  std::string key("0123456789-ABCDEFGHIJ-0123456789");
  auto expected = SourceEncryptionKey::FromBinaryKey(key);
  ASSERT_TRUE(expected.has_value());
  auto actual = SourceEncryptionKey::FromBase64Key(expected.value().key);
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(expected.value().algorithm, actual.value().algorithm);
  EXPECT_EQ(expected.value().key, actual.value().key);
  EXPECT_EQ(expected.value().sha256, actual.value().sha256);
}

/// @test Verify that CreateKeyFromGenerator works as expected.
TEST(WellKnownHeader, FromGenerator) {
  google::cloud::internal::DefaultPRNG gen =
      google::cloud::internal::MakeDefaultPRNG();

  auto header = EncryptionKey(CreateKeyFromGenerator(gen));
  ASSERT_TRUE(header.has_value());
  ASSERT_EQ("AES256", header.value().algorithm);
  ASSERT_FALSE(header.value().key.empty());
  ASSERT_FALSE(header.value().sha256.empty());
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
