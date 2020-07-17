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
#include "google/cloud/status.h"
#include "absl/memory/memory.h"
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
const std::string kEmptyStringCrc32cChecksum = "AAAAAA==";
const std::string kEmptyStringMD5Hash = "1B2M2Y8AsgTpgAmY7PhCfg==";

// /bin/echo -n 'The quick brown fox jumps over the lazy dog' > foo.txt
// gsutil hash foo.txt
const std::string kQuickFoxCrc32cChecksum = "ImIEBA==";
const std::string kQuickFoxMD5Hash = "nhB9nTcrtoJr2B01QqQZ1g==";

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
  validator.ProcessHeader("x-goog-hash", "md5=" + kEmptyStringMD5Hash);
  auto result = std::move(validator).Finish();
  EXPECT_EQ(result.computed, result.received);
  EXPECT_EQ(kEmptyStringMD5Hash, result.computed);
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
  EXPECT_EQ(kQuickFoxMD5Hash, result.computed);
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
  EXPECT_EQ(kQuickFoxMD5Hash, result.computed);
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
  EXPECT_EQ(kQuickFoxMD5Hash, result.computed);
  EXPECT_TRUE(result.is_mismatch);
}

TEST(Crc32cHashValidator, Empty) {
  Crc32cHashValidator validator;
  validator.ProcessHeader("x-goog-hash",
                          "crc32c=" + kEmptyStringCrc32cChecksum);
  validator.ProcessHeader("x-goog-hash", "md5=<invalid-should-be-ignored>");
  auto result = std::move(validator).Finish();
  EXPECT_EQ(result.computed, result.received);
  EXPECT_EQ(kEmptyStringCrc32cChecksum, result.computed);
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
  EXPECT_EQ(kQuickFoxCrc32cChecksum, result.computed);
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
  EXPECT_EQ(kQuickFoxCrc32cChecksum, result.computed);
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
  EXPECT_EQ(kQuickFoxCrc32cChecksum, result.computed);
  EXPECT_TRUE(result.is_mismatch);
}

TEST(CompositeHashValidator, Empty) {
  CompositeValidator validator(absl::make_unique<Crc32cHashValidator>(),
                               absl::make_unique<MD5HashValidator>());
  validator.ProcessHeader("x-goog-hash",
                          "crc32c=" + kEmptyStringCrc32cChecksum);
  validator.ProcessHeader("x-goog-hash", "md5=" + kEmptyStringMD5Hash);
  auto result = std::move(validator).Finish();
  EXPECT_EQ(result.computed, result.received);
  EXPECT_EQ(
      "crc32c=" + kEmptyStringCrc32cChecksum + ",md5=" + kEmptyStringMD5Hash,
      result.computed);
  EXPECT_FALSE(result.is_mismatch);
}

TEST(CompositeHashValidator, Simple) {
  CompositeValidator validator(absl::make_unique<Crc32cHashValidator>(),
                               absl::make_unique<MD5HashValidator>());
  UpdateValidator(validator, "The quick");
  UpdateValidator(validator, " brown");
  UpdateValidator(validator, " fox jumps over the lazy dog");
  validator.ProcessHeader("x-goog-hash", "crc32c=<invalid-crc32c-for-test>");
  validator.ProcessHeader("x-goog-hash", "md5=<invalid-md5-for-test>");
  auto result = std::move(validator).Finish();
  EXPECT_EQ("crc32c=<invalid-crc32c-for-test>,md5=<invalid-md5-for-test>",
            result.received);
  EXPECT_EQ("crc32c=" + kQuickFoxCrc32cChecksum + ",md5=" + kQuickFoxMD5Hash,
            result.computed);
  EXPECT_TRUE(result.is_mismatch);
}

TEST(CompositeHashValidator, ProcessMetadata) {
  CompositeValidator validator(absl::make_unique<Crc32cHashValidator>(),
                               absl::make_unique<MD5HashValidator>());
  UpdateValidator(validator, "The quick");
  UpdateValidator(validator, " brown");
  UpdateValidator(validator, " fox jumps over the lazy dog");
  auto object_metadata = internal::ObjectMetadataParser::FromJson(
                             internal::nl::json{
                                 {"crc32c", kQuickFoxCrc32cChecksum},
                                 {"md5Hash", kQuickFoxMD5Hash},
                             })
                             .value();
  validator.ProcessMetadata(object_metadata);
  auto result = std::move(validator).Finish();
  EXPECT_EQ("crc32c=" + kQuickFoxCrc32cChecksum + ",md5=" + kQuickFoxMD5Hash,
            result.computed);
  EXPECT_EQ("crc32c=" + kQuickFoxCrc32cChecksum + ",md5=" + kQuickFoxMD5Hash,
            result.received);
  EXPECT_FALSE(result.is_mismatch);
}

TEST(CompositeHashValidator, Missing) {
  CompositeValidator validator(absl::make_unique<Crc32cHashValidator>(),
                               absl::make_unique<MD5HashValidator>());
  UpdateValidator(validator, "The quick");
  UpdateValidator(validator, " brown");
  UpdateValidator(validator, " fox jumps over the lazy dog");
  validator.ProcessHeader("x-goog-hash", "crc32c=" + kQuickFoxCrc32cChecksum);
  auto result = std::move(validator).Finish();
  EXPECT_EQ("crc32c=" + kQuickFoxCrc32cChecksum + ",md5=", result.received);
  EXPECT_EQ("crc32c=" + kQuickFoxCrc32cChecksum + ",md5=" + kQuickFoxMD5Hash,
            result.computed);
  EXPECT_FALSE(result.is_mismatch);
}

TEST(CreateHashValidator, ReadNull) {
  auto validator =
      CreateHashValidator(ReadObjectRangeRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableCrc32cChecksum(true),
                                                    DisableMD5Hash(true)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_TRUE(result.computed.empty());
}

TEST(CreateHashValidator, ReadOnlyCrc32c) {
  auto validator =
      CreateHashValidator(ReadObjectRangeRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableMD5Hash(true)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_EQ(kQuickFoxCrc32cChecksum, result.computed);
}

TEST(CreateHashValidator, ReadDisableCrc32cTrue) {
  auto validator = CreateHashValidator(
      ReadObjectRangeRequest("test-bucket", "test-object")
          .set_multiple_options(DisableCrc32cChecksum(true)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_TRUE(result.computed.empty());
}

TEST(CreateHashValidator, ReadWithoutConstructors) {
  auto validator =
      CreateHashValidator(ReadObjectRangeRequest("test-bucket", "test-object"));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_EQ(kQuickFoxCrc32cChecksum, result.computed);
}

TEST(CreateHashValidator, ReadDisableCrc32cFalse) {
  auto validator = CreateHashValidator(
      ReadObjectRangeRequest("test-bucket", "test-object")
          .set_multiple_options(DisableCrc32cChecksum(false)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_EQ(kQuickFoxCrc32cChecksum, result.computed);
}

TEST(CreateHashValidator, ReadDisableMD5False) {
  auto validator =
      CreateHashValidator(ReadObjectRangeRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableMD5Hash(false)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_THAT(result.computed, HasSubstr(kQuickFoxMD5Hash));
  EXPECT_THAT(result.computed, HasSubstr(kQuickFoxCrc32cChecksum));
}

TEST(CreateHashValidator, ReadDisableCrc32cDefaultConstructor) {
  auto validator =
      CreateHashValidator(ReadObjectRangeRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableCrc32cChecksum()));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_EQ(kQuickFoxCrc32cChecksum, result.computed);
}

TEST(CreateHashValidator, ReadDisableMD5DefaultConstructor) {
  auto validator =
      CreateHashValidator(ReadObjectRangeRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableMD5Hash()));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_EQ(kQuickFoxCrc32cChecksum, result.computed);
}

TEST(CreateHashValidator, WriteNull) {
  auto validator =
      CreateHashValidator(ResumableUploadRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableCrc32cChecksum(true),
                                                    DisableMD5Hash(true)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_TRUE(result.computed.empty());
}

TEST(CreateHashValidator, WriteOnlyCrc32c) {
  auto validator =
      CreateHashValidator(ResumableUploadRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableMD5Hash(true)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_EQ(kQuickFoxCrc32cChecksum, result.computed);
}

TEST(CreateHashValidator, WriteDisableCrc32cTrue) {
  auto validator = CreateHashValidator(
      ResumableUploadRequest("test-bucket", "test-object")
          .set_multiple_options(DisableCrc32cChecksum(true)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_TRUE(result.computed.empty());
}

TEST(CreateHashValidator, WriteWithoutConstrutors) {
  auto validator =
      CreateHashValidator(ResumableUploadRequest("test-bucket", "test-object"));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_EQ(kQuickFoxCrc32cChecksum, result.computed);
}

TEST(CreateHashValidator, WriteBothFalse) {
  auto validator = CreateHashValidator(
      ResumableUploadRequest("test-bucket", "test-object")
          .set_multiple_options(DisableCrc32cChecksum(false),
                                DisableMD5Hash(false)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_THAT(result.computed, HasSubstr(kQuickFoxMD5Hash));
  EXPECT_THAT(result.computed, HasSubstr(kQuickFoxCrc32cChecksum));
}

TEST(CreateHashValidator, WriteBothDefaultConstructor) {
  auto validator = CreateHashValidator(
      ResumableUploadRequest("test-bucket", "test-object")
          .set_multiple_options(DisableCrc32cChecksum(), DisableMD5Hash()));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_EQ(kQuickFoxCrc32cChecksum, result.computed);
}

TEST(CreateHashValidator, WriteDisableCrc32False) {
  auto validator = CreateHashValidator(
      ResumableUploadRequest("test-bucket", "test-object")
          .set_multiple_options(DisableCrc32cChecksum(false)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_EQ(kQuickFoxCrc32cChecksum, result.computed);
}

TEST(CreateHashValidator, WriteDisableMD5False) {
  auto validator =
      CreateHashValidator(ResumableUploadRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableMD5Hash(false)));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_THAT(result.computed, HasSubstr(kQuickFoxMD5Hash));
  EXPECT_THAT(result.computed, HasSubstr(kQuickFoxCrc32cChecksum));
}

TEST(CreateHashValidator, WriteDisableCrc32cDefaultConstructor) {
  auto validator =
      CreateHashValidator(ResumableUploadRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableCrc32cChecksum()));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_EQ(kQuickFoxCrc32cChecksum, result.computed);
}

TEST(CreateHashValidator, WriteDisableMD5DefaultConstructor) {
  auto validator =
      CreateHashValidator(ResumableUploadRequest("test-bucket", "test-object")
                              .set_multiple_options(DisableMD5Hash()));
  UpdateValidator(*validator, "The quick brown fox jumps over the lazy dog");
  auto result = std::move(*validator).Finish();
  EXPECT_EQ(kQuickFoxCrc32cChecksum, result.computed);
}
}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
