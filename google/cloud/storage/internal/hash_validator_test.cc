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

#include "google/cloud/storage/internal/hash_validator.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/storage/status.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
using ::testing::HasSubstr;

TEST(HashValidator, CheckResultMismatch) {
  HashValidator::Result result{"received-hash", "computed-hash"};
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(try { HashValidator::CheckResult("test-msg", result); } catch (
                   google::cloud::storage::HashMismatchError const& ex) {
    EXPECT_EQ("received-hash", ex.received_hash());
    EXPECT_EQ("computed-hash", ex.computed_hash());
    EXPECT_THAT(ex.what(), HasSubstr("test-msg"));
    throw;
  },
               std::ios::failure);
#else
  // The program should not be terminated in this case.
  HashValidator::CheckResult("test-msg", result);
  SUCCEED();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(HashValidator, CheckResultMatch) {
  HashValidator::Result result{"test-hash", "test-hash"};
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_NO_THROW(HashValidator::CheckResult("test-msg", result));
#else
  // The program should not be terminated in this case.
  HashValidator::CheckResult("test-msg", result);
  SUCCEED();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(HashValidator, CheckResultEmptyReceived) {
  // Sometimes the service does not send the hashes back, we do not treat that
  // as a hash mismatch.
  HashValidator::Result result{"", "test-hash"};
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_NO_THROW(HashValidator::CheckResult("test-msg", result));
#else
  // The program should not be terminated in this case.
  HashValidator::CheckResult("test-msg", result);
  SUCCEED();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(HashValidator, FinishAndCheck) {
  NullHashValidator validator;
  validator.Update("");
  validator.ProcessHeader("x-goog-hash", "md5=1B2M2Y8AsgTpgAmY7PhCfg==");
  auto result = HashValidator::FinishAndCheck("test-msg", std::move(validator));
  EXPECT_TRUE(result.received.empty());
  EXPECT_TRUE(result.computed.empty());
}

TEST(HashValidator, FinishAndCheckMismatch) {
  MD5HashValidator validator;
  validator.Update("The quick brown fox jumps over the lazy dog");
  // This is the hash for an empty message, computed using:
  // /bin/echo -n "" | openssl md5 -binary | openssl base64
  validator.ProcessHeader("x-goog-hash", "md5=1B2M2Y8AsgTpgAmY7PhCfg==");
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try {
        HashValidator::FinishAndCheck("test-msg", std::move(validator));
      } catch (google::cloud::storage::HashMismatchError const& ex) {
        EXPECT_EQ("1B2M2Y8AsgTpgAmY7PhCfg==", ex.received_hash());
        EXPECT_EQ("nhB9nTcrtoJr2B01QqQZ1g==", ex.computed_hash());
        EXPECT_THAT(ex.what(), HasSubstr("test-msg"));
        throw;
      },
      std::ios::failure);
#else
  auto result = HashValidator::FinishAndCheck("test-msg", std::move(validator));
  EXPECT_EQ("1B2M2Y8AsgTpgAmY7PhCfg==", result.received);
  EXPECT_EQ("nhB9nTcrtoJr2B01QqQZ1g==", result.computed);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(NullHashValidatorTest, Simple) {
  NullHashValidator validator;
  validator.ProcessHeader("x-goog-hash", "md5=<placeholder-for-test>");
  validator.Update("The quick");
  validator.Update(" brown");
  validator.Update(" fox jumps over the lazy dog");
  auto result = std::move(validator).Finish();
  EXPECT_TRUE(result.computed.empty());
  EXPECT_TRUE(result.received.empty());
}

TEST(MD5HashValidator, Empty) {
  MD5HashValidator validator;
  // Use this command to get the expected value:
  // /bin/echo -n "" | openssl md5 -binary | openssl base64
  validator.ProcessHeader("x-goog-hash", "md5=1B2M2Y8AsgTpgAmY7PhCfg==");
  auto result = std::move(validator).Finish();
  EXPECT_EQ(result.computed, result.received);
  EXPECT_EQ("1B2M2Y8AsgTpgAmY7PhCfg==", result.computed);
}

TEST(MD5HashValidator, Simple) {
  MD5HashValidator validator;
  validator.Update("The quick");
  validator.Update(" brown");
  validator.Update(" fox jumps over the lazy dog");
  validator.ProcessHeader("x-goog-hash", "md5=<invalid-value-for-test>");
  // I used this command to get the expected value:
  // /bin/echo -n "The quick brown fox jumps over the lazy dog" |
  //     openssl md5 -binary | openssl base64
  auto result = std::move(validator).Finish();
  EXPECT_EQ("<invalid-value-for-test>", result.received);
  EXPECT_EQ("nhB9nTcrtoJr2B01QqQZ1g==", result.computed);
}

TEST(MD5HashValidator, MultipleHashesMd5AtEnd) {
  MD5HashValidator validator;
  validator.Update("The quick");
  validator.Update(" brown");
  validator.Update(" fox jumps over the lazy dog");
  validator.ProcessHeader(
      "x-goog-hash", "crc32c=<should-be-ignored>,md5=<invalid-value-for-test>");
  // I used this command to get the expected value:
  // /bin/echo -n "The quick brown fox jumps over the lazy dog" |
  //     openssl md5 -binary | openssl base64
  auto result = std::move(validator).Finish();
  EXPECT_EQ("<invalid-value-for-test>", result.received);
  EXPECT_EQ("nhB9nTcrtoJr2B01QqQZ1g==", result.computed);
}

TEST(MD5HashValidator, MultipleHashes) {
  MD5HashValidator validator;
  validator.Update("The quick");
  validator.Update(" brown");
  validator.Update(" fox jumps over the lazy dog");
  validator.ProcessHeader(
      "x-goog-hash", "md5=<invalid-value-for-test>,crc32c=<should-be-ignored>");
  // I used this command to get the expected value:
  // /bin/echo -n "The quick brown fox jumps over the lazy dog" |
  //     openssl md5 -binary | openssl base64
  auto result = std::move(validator).Finish();
  EXPECT_EQ("<invalid-value-for-test>", result.received);
  EXPECT_EQ("nhB9nTcrtoJr2B01QqQZ1g==", result.computed);
}

TEST(Crc32cHashValidator, Empty) {
  Crc32cHashValidator validator;
  // Use this command to get the expected value:
  // echo -n '' > foo.txt && gsutil hash foo.txt
  validator.ProcessHeader("x-goog-hash", "crc32c=AAAAAA==");
  validator.ProcessHeader("x-goog-hash", "md5=AAAAAA==");
  auto result = std::move(validator).Finish();
  EXPECT_EQ(result.computed, result.received);
  EXPECT_EQ("AAAAAA==", result.computed);
}

TEST(Crc32cHashValidator, Simple) {
  Crc32cHashValidator validator;
  validator.Update("The quick");
  validator.Update(" brown");
  validator.Update(" fox jumps over the lazy dog");
  validator.ProcessHeader("x-goog-hash", "crc32c=<invalid-value-for-test>");
  // I used this command to get the expected value:
  // /bin/echo -n "The quick brown fox jumps over the lazy dog" > foo.txt
  // gsutil hash foo.txt
  auto result = std::move(validator).Finish();
  EXPECT_EQ("<invalid-value-for-test>", result.received);
  EXPECT_EQ("ImIEBA==", result.computed);
}

TEST(Crc32cHashValidator, MultipleHashesCrc32cAtEnd) {
  Crc32cHashValidator validator;
  validator.Update("The quick");
  validator.Update(" brown");
  validator.Update(" fox jumps over the lazy dog");
  validator.ProcessHeader("x-goog-hash",
                          "md5=<ignored>,crc32c=<invalid-value-for-test>");
  // I used this command to get the expected value:
  // /bin/echo -n "The quick brown fox jumps over the lazy dog" > foo.txt
  // gsutil hash foo.txt
  auto result = std::move(validator).Finish();
  EXPECT_EQ("<invalid-value-for-test>", result.received);
  EXPECT_EQ("ImIEBA==", result.computed);
}

TEST(Crc32cHashValidator, MultipleHashes) {
  Crc32cHashValidator validator;
  validator.Update("The quick");
  validator.Update(" brown");
  validator.Update(" fox jumps over the lazy dog");
  validator.ProcessHeader("x-goog-hash",
                          "crc32c=<invalid-value-for-test>,md5=<ignored>");
  // I used this command to get the expected value:
  // /bin/echo -n "The quick brown fox jumps over the lazy dog" > foo.txt
  // gsutil hash foo.txt
  auto result = std::move(validator).Finish();
  EXPECT_EQ("<invalid-value-for-test>", result.received);
  EXPECT_EQ("ImIEBA==", result.computed);
}

TEST(CompositeHashValidator, Empty) {
  CompositeValidator validator(
      google::cloud::internal::make_unique<Crc32cHashValidator>(),
      google::cloud::internal::make_unique<MD5HashValidator>());
  // Look at the tests for each validator to find out where the magic numbers
  // come from.
  validator.ProcessHeader("x-goog-hash", "crc32c=AAAAAA==");
  validator.ProcessHeader("x-goog-hash", "md5=1B2M2Y8AsgTpgAmY7PhCfg==");
  auto result = std::move(validator).Finish();
  EXPECT_EQ(result.computed, result.received);
  EXPECT_EQ("crc32c=AAAAAA==,md5=1B2M2Y8AsgTpgAmY7PhCfg==", result.computed);
}

TEST(CompositeHashValidator, Simple) {
  CompositeValidator validator(
      google::cloud::internal::make_unique<Crc32cHashValidator>(),
      google::cloud::internal::make_unique<MD5HashValidator>());
  validator.Update("The quick");
  validator.Update(" brown");
  validator.Update(" fox jumps over the lazy dog");
  validator.ProcessHeader("x-goog-hash", "crc32c=<invalid-crc32c-for-test>");
  validator.ProcessHeader("x-goog-hash", "md5=<invalid-md5-for-test>");
  // Look at the tests for each validator to find out where the magic numbers
  // come from.
  auto result = std::move(validator).Finish();
  EXPECT_EQ("crc32c=<invalid-crc32c-for-test>,md5=<invalid-md5-for-test>",
            result.received);
  EXPECT_EQ("crc32c=ImIEBA==,md5=nhB9nTcrtoJr2B01QqQZ1g==", result.computed);
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
