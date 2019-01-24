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
#include "google/cloud/status.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/object_metadata.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
using ::testing::HasSubstr;

// These values were obtained using:
// echo -n '' > foo.txt && gsutil hash foo.txt
const std::string EMPTY_STRING_CRC32C_CHECKSUM = "AAAAAA==";
const std::string EMPTY_STRING_MD5_HASH = "1B2M2Y8AsgTpgAmY7PhCfg==";

// /bin/echo -n 'The quick brown fox jumps over the lazy dog' > foo.txt
// gsutil hash foo.txt
const std::string QUICK_FOX_CRC32C_CHECKSUM = "ImIEBA==";
const std::string QUICK_FOX_MD5_HASH = "nhB9nTcrtoJr2B01QqQZ1g==";

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
  validator.ProcessHeader("x-goog-hash", "md5=" + EMPTY_STRING_MD5_HASH);
  auto result = std::move(validator).Finish();
  EXPECT_EQ(result.computed, result.received);
  EXPECT_EQ(EMPTY_STRING_MD5_HASH, result.computed);
  EXPECT_FALSE(result.is_mismatch);
}

TEST(MD5HashValidator, Simple) {
  MD5HashValidator validator;
  validator.Update("The quick");
  validator.Update(" brown");
  validator.Update(" fox jumps over the lazy dog");
  validator.ProcessHeader("x-goog-hash", "md5=<invalid-value-for-test>");
  auto result = std::move(validator).Finish();
  EXPECT_EQ("<invalid-value-for-test>", result.received);
  EXPECT_EQ(QUICK_FOX_MD5_HASH, result.computed);
  EXPECT_TRUE(result.is_mismatch);
}

TEST(MD5HashValidator, MultipleHashesMd5AtEnd) {
  MD5HashValidator validator;
  validator.Update("The quick");
  validator.Update(" brown");
  validator.Update(" fox jumps over the lazy dog");
  validator.ProcessHeader(
      "x-goog-hash", "crc32c=<should-be-ignored>,md5=<invalid-value-for-test>");
  auto result = std::move(validator).Finish();
  EXPECT_EQ("<invalid-value-for-test>", result.received);
  EXPECT_EQ(QUICK_FOX_MD5_HASH, result.computed);
  EXPECT_TRUE(result.is_mismatch);
}

TEST(MD5HashValidator, MultipleHashes) {
  MD5HashValidator validator;
  validator.Update("The quick");
  validator.Update(" brown");
  validator.Update(" fox jumps over the lazy dog");
  validator.ProcessHeader(
      "x-goog-hash", "md5=<invalid-value-for-test>,crc32c=<should-be-ignored>");
  auto result = std::move(validator).Finish();
  EXPECT_EQ("<invalid-value-for-test>", result.received);
  EXPECT_EQ(QUICK_FOX_MD5_HASH, result.computed);
  EXPECT_TRUE(result.is_mismatch);
}

TEST(Crc32cHashValidator, Empty) {
  Crc32cHashValidator validator;
  validator.ProcessHeader("x-goog-hash",
                          "crc32c=" + EMPTY_STRING_CRC32C_CHECKSUM);
  validator.ProcessHeader("x-goog-hash", "md5=<invalid-should-be-ignored>");
  auto result = std::move(validator).Finish();
  EXPECT_EQ(result.computed, result.received);
  EXPECT_EQ(EMPTY_STRING_CRC32C_CHECKSUM, result.computed);
  EXPECT_FALSE(result.is_mismatch);
}

TEST(Crc32cHashValidator, Simple) {
  Crc32cHashValidator validator;
  validator.Update("The quick");
  validator.Update(" brown");
  validator.Update(" fox jumps over the lazy dog");
  validator.ProcessHeader("x-goog-hash", "crc32c=<invalid-value-for-test>");
  auto result = std::move(validator).Finish();
  EXPECT_EQ("<invalid-value-for-test>", result.received);
  EXPECT_EQ(QUICK_FOX_CRC32C_CHECKSUM, result.computed);
  EXPECT_TRUE(result.is_mismatch);
}

TEST(Crc32cHashValidator, MultipleHashesCrc32cAtEnd) {
  Crc32cHashValidator validator;
  validator.Update("The quick");
  validator.Update(" brown");
  validator.Update(" fox jumps over the lazy dog");
  validator.ProcessHeader("x-goog-hash",
                          "md5=<ignored>,crc32c=<invalid-value-for-test>");
  auto result = std::move(validator).Finish();
  EXPECT_EQ("<invalid-value-for-test>", result.received);
  EXPECT_EQ(QUICK_FOX_CRC32C_CHECKSUM, result.computed);
  EXPECT_TRUE(result.is_mismatch);
}

TEST(Crc32cHashValidator, MultipleHashes) {
  Crc32cHashValidator validator;
  validator.Update("The quick");
  validator.Update(" brown");
  validator.Update(" fox jumps over the lazy dog");
  validator.ProcessHeader("x-goog-hash",
                          "crc32c=<invalid-value-for-test>,md5=<ignored>");
  auto result = std::move(validator).Finish();
  EXPECT_EQ("<invalid-value-for-test>", result.received);
  EXPECT_EQ(QUICK_FOX_CRC32C_CHECKSUM, result.computed);
  EXPECT_TRUE(result.is_mismatch);
}

TEST(CompositeHashValidator, Empty) {
  CompositeValidator validator(
      google::cloud::internal::make_unique<Crc32cHashValidator>(),
      google::cloud::internal::make_unique<MD5HashValidator>());
  validator.ProcessHeader("x-goog-hash",
                          "crc32c=" + EMPTY_STRING_CRC32C_CHECKSUM);
  validator.ProcessHeader("x-goog-hash", "md5=" + EMPTY_STRING_MD5_HASH);
  auto result = std::move(validator).Finish();
  EXPECT_EQ(result.computed, result.received);
  EXPECT_EQ("crc32c=" + EMPTY_STRING_CRC32C_CHECKSUM +
                ",md5=" + EMPTY_STRING_MD5_HASH,
            result.computed);
  EXPECT_FALSE(result.is_mismatch);
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
  auto result = std::move(validator).Finish();
  EXPECT_EQ("crc32c=<invalid-crc32c-for-test>,md5=<invalid-md5-for-test>",
            result.received);
  EXPECT_EQ(
      "crc32c=" + QUICK_FOX_CRC32C_CHECKSUM + ",md5=" + QUICK_FOX_MD5_HASH,
      result.computed);
  EXPECT_TRUE(result.is_mismatch);
}

TEST(CompositeHashValidator, ProcessMetadata) {
  CompositeValidator validator(
      google::cloud::internal::make_unique<Crc32cHashValidator>(),
      google::cloud::internal::make_unique<MD5HashValidator>());
  validator.Update("The quick");
  validator.Update(" brown");
  validator.Update(" fox jumps over the lazy dog");
  auto object_metadata = internal::ObjectMetadataParser::FromJson(
                             internal::nl::json{
                                 {"crc32c", QUICK_FOX_CRC32C_CHECKSUM},
                                 {"md5Hash", QUICK_FOX_MD5_HASH},
                             })
                             .value();
  validator.ProcessMetadata(object_metadata);
  auto result = std::move(validator).Finish();
  EXPECT_EQ(
      "crc32c=" + QUICK_FOX_CRC32C_CHECKSUM + ",md5=" + QUICK_FOX_MD5_HASH,
      result.computed);
  EXPECT_EQ(
      "crc32c=" + QUICK_FOX_CRC32C_CHECKSUM + ",md5=" + QUICK_FOX_MD5_HASH,
      result.received);
  EXPECT_FALSE(result.is_mismatch);
}

TEST(CompositeHashValidator, Missing) {
  CompositeValidator validator(
      google::cloud::internal::make_unique<Crc32cHashValidator>(),
      google::cloud::internal::make_unique<MD5HashValidator>());
  validator.Update("The quick");
  validator.Update(" brown");
  validator.Update(" fox jumps over the lazy dog");
  validator.ProcessHeader("x-goog-hash", "crc32c=" + QUICK_FOX_CRC32C_CHECKSUM);
  auto result = std::move(validator).Finish();
  EXPECT_EQ("crc32c=" + QUICK_FOX_CRC32C_CHECKSUM + ",md5=", result.received);
  EXPECT_EQ(
      "crc32c=" + QUICK_FOX_CRC32C_CHECKSUM + ",md5=" + QUICK_FOX_MD5_HASH,
      result.computed);
  EXPECT_FALSE(result.is_mismatch);
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
