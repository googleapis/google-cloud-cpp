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
#include "google/cloud/storage/internal/hash_validator_impl.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/status.h"
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

void UpdateValidator(HashValidator& validator, std::string const& buffer) {
  validator.Update(buffer.data(), buffer.size());
}

TEST(NullHashValidatorTest, Simple) {
  NullHashValidator validator;
  validator.ProcessHeader("x-goog-hash", "md5=<placeholder-for-test>");
  UpdateValidator(validator, "The quick");
  UpdateValidator(validator, " brown");
  UpdateValidator(validator, " fox jumps over the lazy dog");
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
  UpdateValidator(validator, "The quick");
  UpdateValidator(validator, " brown");
  UpdateValidator(validator, " fox jumps over the lazy dog");
  validator.ProcessHeader("x-goog-hash", "md5=<invalid-value-for-test>");
  auto result = std::move(validator).Finish();
  EXPECT_EQ("<invalid-value-for-test>", result.received);
  EXPECT_EQ(QUICK_FOX_MD5_HASH, result.computed);
  EXPECT_TRUE(result.is_mismatch);
}

TEST(MD5HashValidator, MultipleHashesMd5AtEnd) {
  MD5HashValidator validator;
  UpdateValidator(validator, "The quick");
  UpdateValidator(validator, " brown");
  UpdateValidator(validator, " fox jumps over the lazy dog");
  validator.ProcessHeader(
      "x-goog-hash", "crc32c=<should-be-ignored>,md5=<invalid-value-for-test>");
  auto result = std::move(validator).Finish();
  EXPECT_EQ("<invalid-value-for-test>", result.received);
  EXPECT_EQ(QUICK_FOX_MD5_HASH, result.computed);
  EXPECT_TRUE(result.is_mismatch);
}

TEST(MD5HashValidator, MultipleHashes) {
  MD5HashValidator validator;
  UpdateValidator(validator, "The quick");
  UpdateValidator(validator, " brown");
  UpdateValidator(validator, " fox jumps over the lazy dog");
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
  UpdateValidator(validator, "The quick");
  UpdateValidator(validator, " brown");
  UpdateValidator(validator, " fox jumps over the lazy dog");
  validator.ProcessHeader("x-goog-hash", "crc32c=<invalid-value-for-test>");
  auto result = std::move(validator).Finish();
  EXPECT_EQ("<invalid-value-for-test>", result.received);
  EXPECT_EQ(QUICK_FOX_CRC32C_CHECKSUM, result.computed);
  EXPECT_TRUE(result.is_mismatch);
}

TEST(Crc32cHashValidator, MultipleHashesCrc32cAtEnd) {
  Crc32cHashValidator validator;
  UpdateValidator(validator, "The quick");
  UpdateValidator(validator, " brown");
  UpdateValidator(validator, " fox jumps over the lazy dog");
  validator.ProcessHeader("x-goog-hash",
                          "md5=<ignored>,crc32c=<invalid-value-for-test>");
  auto result = std::move(validator).Finish();
  EXPECT_EQ("<invalid-value-for-test>", result.received);
  EXPECT_EQ(QUICK_FOX_CRC32C_CHECKSUM, result.computed);
  EXPECT_TRUE(result.is_mismatch);
}

TEST(Crc32cHashValidator, MultipleHashes) {
  Crc32cHashValidator validator;
  UpdateValidator(validator, "The quick");
  UpdateValidator(validator, " brown");
  UpdateValidator(validator, " fox jumps over the lazy dog");
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
  UpdateValidator(validator, "The quick");
  UpdateValidator(validator, " brown");
  UpdateValidator(validator, " fox jumps over the lazy dog");
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
  UpdateValidator(validator, "The quick");
  UpdateValidator(validator, " brown");
  UpdateValidator(validator, " fox jumps over the lazy dog");
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
  UpdateValidator(validator, "The quick");
  UpdateValidator(validator, " brown");
  UpdateValidator(validator, " fox jumps over the lazy dog");
  validator.ProcessHeader("x-goog-hash", "crc32c=" + QUICK_FOX_CRC32C_CHECKSUM);
  auto result = std::move(validator).Finish();
  EXPECT_EQ("crc32c=" + QUICK_FOX_CRC32C_CHECKSUM + ",md5=", result.received);
  EXPECT_EQ(
      "crc32c=" + QUICK_FOX_CRC32C_CHECKSUM + ",md5=" + QUICK_FOX_MD5_HASH,
      result.computed);
  EXPECT_FALSE(result.is_mismatch);
}

TEST(CreateHashValidator, Read_Null) {
  auto validator =
      CreateHashValidator(ReadObjectRangeRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableCrc32cChecksum(true),
                                                    DisableMD5Hash(true)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_TRUE(result.computed.empty());
}

TEST(CreateHashValidator, Read_OnlyCrc32c) {
  auto validator =
      CreateHashValidator(ReadObjectRangeRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableMD5Hash(true)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_EQ(QUICK_FOX_CRC32C_CHECKSUM, result.computed);
}

TEST(CreateHashValidator, Read_OnlyMD5) {
  auto validator = CreateHashValidator(
      ReadObjectRangeRequest("test-bucket", "test-object")
          .set_multiple_options(DisableCrc32cChecksum(true)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_EQ(QUICK_FOX_MD5_HASH, result.computed);
}

TEST(CreateHashValidator, Read_Both) {
  auto validator =
      CreateHashValidator(ReadObjectRangeRequest("test-bucket", "test-object"));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_MD5_HASH));
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_CRC32C_CHECKSUM));
}

TEST(CreateHashValidator, Read_DisableCrc32c_False) {
  auto validator = CreateHashValidator(
      ReadObjectRangeRequest("test-bucket", "test-object")
          .set_multiple_options(DisableCrc32cChecksum(false)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_MD5_HASH));
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_CRC32C_CHECKSUM));
}

TEST(CreateHashValidator, Read_DisableMD5_False) {
  auto validator =
      CreateHashValidator(ReadObjectRangeRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableMD5Hash(false)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_MD5_HASH));
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_CRC32C_CHECKSUM));
}

TEST(CreateHashValidator, Read_DisableCrc32c_Default_Constructor) {
  auto validator =
      CreateHashValidator(ReadObjectRangeRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableCrc32cChecksum()));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_MD5_HASH));
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_CRC32C_CHECKSUM));
}

TEST(CreateHashValidator, Read_DisableMD5_Default_Constructor) {
  auto validator =
      CreateHashValidator(ReadObjectRangeRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableMD5Hash()));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_MD5_HASH));
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_CRC32C_CHECKSUM));
}

TEST(CreateHashValidator, Write_Null) {
  auto validator =
      CreateHashValidator(ResumableUploadRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableCrc32cChecksum(true),
                                                    DisableMD5Hash(true)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_TRUE(result.computed.empty());
}

TEST(CreateHashValidator, Write_OnlyCrc32c) {
  auto validator =
      CreateHashValidator(ResumableUploadRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableMD5Hash(true)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_EQ(QUICK_FOX_CRC32C_CHECKSUM, result.computed);
}

TEST(CreateHashValidator, Write_OnlyMD5) {
  auto validator = CreateHashValidator(
      ResumableUploadRequest("test-bucket", "test-object")
          .set_multiple_options(DisableCrc32cChecksum(true)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_EQ(QUICK_FOX_MD5_HASH, result.computed);
}

TEST(CreateHashValidator, Write_Both) {
  auto validator =
      CreateHashValidator(ResumableUploadRequest("test-bucket", "test-object"));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_MD5_HASH));
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_CRC32C_CHECKSUM));
}

TEST(CreateHashValidator, Write_Both_False) {
  auto validator = CreateHashValidator(
      ResumableUploadRequest("test-bucket", "test-object")
          .set_multiple_options(DisableCrc32cChecksum(false),
                                DisableMD5Hash(false)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_MD5_HASH));
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_CRC32C_CHECKSUM));
}

TEST(CreateHashValidator, Write_Both_Default_Constructor) {
  auto validator = CreateHashValidator(
      ResumableUploadRequest("test-bucket", "test-object")
          .set_multiple_options(DisableCrc32cChecksum(), DisableMD5Hash()));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_MD5_HASH));
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_CRC32C_CHECKSUM));
}

TEST(CreateHashValidator, Write_DisableCrc32_False) {
  auto validator = CreateHashValidator(
      ResumableUploadRequest("test-bucket", "test-object")
          .set_multiple_options(DisableCrc32cChecksum(false)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_MD5_HASH));
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_CRC32C_CHECKSUM));
}

TEST(CreateHashValidator, Write_DisableMD5_False) {
  auto validator =
      CreateHashValidator(ResumableUploadRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableMD5Hash(false)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_MD5_HASH));
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_CRC32C_CHECKSUM));
}

TEST(CreateHashValidator, Write_DisableCrc32c_Default_Constructor) {
  auto validator =
      CreateHashValidator(ResumableUploadRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableCrc32cChecksum()));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_MD5_HASH));
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_CRC32C_CHECKSUM));
}

TEST(CreateHashValidator, Write_DisableMD5_Default_Constructor) {
  auto validator =
      CreateHashValidator(ResumableUploadRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableMD5Hash()));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_MD5_HASH));
  EXPECT_THAT(result.computed, HasSubstr(QUICK_FOX_CRC32C_CHECKSUM));
}
}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
