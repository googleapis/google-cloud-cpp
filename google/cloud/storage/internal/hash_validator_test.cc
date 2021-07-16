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
#include "google/cloud/storage/internal/hash_function_impl.h"
#include "google/cloud/storage/internal/hash_validator_impl.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::testing::IsEmpty;

// These values were obtained using:
// echo -n '' > foo.txt && gsutil hash foo.txt
auto constexpr kEmptyStringCrc32cChecksum = "AAAAAA==";
auto constexpr kEmptyStringMD5Hash = "1B2M2Y8AsgTpgAmY7PhCfg==";

// /bin/echo -n 'The quick brown fox jumps over the lazy dog' > foo.txt
// gsutil hash foo.txt
auto constexpr kQuickFoxCrc32cChecksum = "ImIEBA==";
auto constexpr kQuickFoxMD5Hash = "nhB9nTcrtoJr2B01QqQZ1g==";

HashValues HashEmpty(std::unique_ptr<HashFunction> function) {
  return std::move(*function).Finish();
}

HashValues HashQuick(std::unique_ptr<HashFunction> function) {
  auto const text = std::string{"The quick brown fox jumps over the lazy dog"};
  function->Update(text.data(), text.size());
  return std::move(*function).Finish();
}

TEST(NullHashValidatorTest, Simple) {
  NullHashValidator validator;
  validator.ProcessHeader("x-goog-hash", "md5=<placeholder-for-test>");
  auto result =
      std::move(validator).Finish(HashQuick(CreateNullHashFunction()));
  EXPECT_THAT(result.received.crc32c, IsEmpty());
  EXPECT_THAT(result.received.md5, IsEmpty());
}

TEST(MD5HashValidator, Empty) {
  MD5HashValidator validator;
  validator.ProcessHeader("x-goog-hash",
                          "md5=" + std::string{kEmptyStringMD5Hash});
  auto result = std::move(validator).Finish(
      HashEmpty(absl::make_unique<MD5HashFunction>()));
  EXPECT_THAT(result.received.crc32c, IsEmpty());
  EXPECT_THAT(result.received.md5, kEmptyStringMD5Hash);
  EXPECT_EQ(result.computed.crc32c, result.received.crc32c);
  EXPECT_EQ(result.computed.md5, result.received.md5);
  EXPECT_FALSE(result.is_mismatch);
}

TEST(MD5HashValidator, Simple) {
  MD5HashValidator validator;
  validator.ProcessHeader("x-goog-hash", "md5=<invalid-value-for-test>");
  auto result = std::move(validator).Finish(
      HashQuick(absl::make_unique<MD5HashFunction>()));
  EXPECT_THAT(result.received.crc32c, IsEmpty());
  EXPECT_THAT(result.received.md5, "<invalid-value-for-test>");
  EXPECT_TRUE(result.is_mismatch);
}

TEST(MD5HashValidator, MultipleHashesMd5AtEnd) {
  MD5HashValidator validator;
  validator.ProcessHeader(
      "x-goog-hash", "crc32c=<should-be-ignored>,md5=<invalid-value-for-test>");
  auto result = std::move(validator).Finish(
      HashQuick(absl::make_unique<MD5HashFunction>()));
  EXPECT_THAT(result.received.crc32c, IsEmpty());
  EXPECT_THAT(result.received.md5, "<invalid-value-for-test>");
  EXPECT_TRUE(result.is_mismatch);
}

TEST(MD5HashValidator, MultipleHashes) {
  MD5HashValidator validator;
  validator.ProcessHeader(
      "x-goog-hash", "md5=<invalid-value-for-test>,crc32c=<should-be-ignored>");
  auto result = std::move(validator).Finish(
      HashQuick(absl::make_unique<MD5HashFunction>()));
  EXPECT_THAT(result.received.crc32c, IsEmpty());
  EXPECT_THAT(result.received.md5, "<invalid-value-for-test>");
  EXPECT_TRUE(result.is_mismatch);
}

TEST(Crc32cHashValidator, Empty) {
  Crc32cHashValidator validator;
  validator.ProcessHeader("x-goog-hash",
                          "crc32c=" + std::string{kEmptyStringCrc32cChecksum});
  validator.ProcessHeader("x-goog-hash", "md5=<invalid-should-be-ignored>");
  auto result = std::move(validator).Finish(
      HashEmpty(absl::make_unique<Crc32cHashFunction>()));
  EXPECT_THAT(result.received.crc32c, kEmptyStringCrc32cChecksum);
  EXPECT_THAT(result.received.md5, IsEmpty());
  EXPECT_FALSE(result.is_mismatch);
}

TEST(Crc32cHashValidator, Simple) {
  Crc32cHashValidator validator;
  validator.ProcessHeader("x-goog-hash", "crc32c=<invalid-value-for-test>");
  auto result = std::move(validator).Finish(
      HashQuick(absl::make_unique<Crc32cHashFunction>()));
  EXPECT_THAT(result.received.crc32c, "<invalid-value-for-test>");
  EXPECT_THAT(result.received.md5, IsEmpty());
  EXPECT_TRUE(result.is_mismatch);
}

TEST(Crc32cHashValidator, MultipleHashesCrc32cAtEnd) {
  Crc32cHashValidator validator;
  validator.ProcessHeader("x-goog-hash",
                          "md5=<ignored>,crc32c=<invalid-value-for-test>");
  auto result = std::move(validator).Finish(
      HashQuick(absl::make_unique<Crc32cHashFunction>()));
  EXPECT_THAT(result.received.crc32c, "<invalid-value-for-test>");
  EXPECT_THAT(result.received.md5, IsEmpty());
  EXPECT_TRUE(result.is_mismatch);
}

TEST(Crc32cHashValidator, MultipleHashes) {
  Crc32cHashValidator validator;
  validator.ProcessHeader("x-goog-hash",
                          "crc32c=<invalid-value-for-test>,md5=<ignored>");
  auto result = std::move(validator).Finish(
      HashQuick(absl::make_unique<Crc32cHashFunction>()));
  EXPECT_THAT(result.received.crc32c, "<invalid-value-for-test>");
  EXPECT_THAT(result.received.md5, IsEmpty());
  EXPECT_TRUE(result.is_mismatch);
}

TEST(CompositeHashValidator, Empty) {
  CompositeValidator validator(absl::make_unique<Crc32cHashValidator>(),
                               absl::make_unique<MD5HashValidator>());
  validator.ProcessHeader("x-goog-hash",
                          "crc32c=" + std::string{kEmptyStringCrc32cChecksum});
  validator.ProcessHeader("x-goog-hash",
                          "md5=" + std::string{kEmptyStringMD5Hash});
  auto result = std::move(validator).Finish(
      HashEmpty(absl::make_unique<CompositeFunction>(
          absl::make_unique<Crc32cHashFunction>(),
          absl::make_unique<MD5HashFunction>())));
  EXPECT_THAT(result.received.crc32c, kEmptyStringCrc32cChecksum);
  EXPECT_THAT(result.received.md5, kEmptyStringMD5Hash);
  EXPECT_FALSE(result.is_mismatch);
}

TEST(CompositeHashValidator, Simple) {
  CompositeValidator validator(absl::make_unique<Crc32cHashValidator>(),
                               absl::make_unique<MD5HashValidator>());
  validator.ProcessHeader("x-goog-hash", "crc32c=<invalid-crc32c-for-test>");
  validator.ProcessHeader("x-goog-hash", "md5=<invalid-md5-for-test>");
  auto result = std::move(validator).Finish(
      HashQuick(absl::make_unique<CompositeFunction>(
          absl::make_unique<Crc32cHashFunction>(),
          absl::make_unique<MD5HashFunction>())));
  EXPECT_THAT(result.received.crc32c, "<invalid-crc32c-for-test>");
  EXPECT_THAT(result.received.md5, "<invalid-md5-for-test>");
  EXPECT_TRUE(result.is_mismatch);
}

TEST(CompositeHashValidator, ProcessMetadata) {
  CompositeValidator validator(absl::make_unique<Crc32cHashValidator>(),
                               absl::make_unique<MD5HashValidator>());
  auto object_metadata = internal::ObjectMetadataParser::FromJson(
                             nlohmann::json{
                                 {"crc32c", kQuickFoxCrc32cChecksum},
                                 {"md5Hash", kQuickFoxMD5Hash},
                             })
                             .value();
  validator.ProcessMetadata(object_metadata);
  auto result = std::move(validator).Finish(
      HashQuick(absl::make_unique<CompositeFunction>(
          absl::make_unique<Crc32cHashFunction>(),
          absl::make_unique<MD5HashFunction>())));
  EXPECT_THAT(result.received.crc32c, kQuickFoxCrc32cChecksum);
  EXPECT_THAT(result.received.md5, kQuickFoxMD5Hash);
  EXPECT_FALSE(result.is_mismatch);
}

TEST(CompositeHashValidator, Missing) {
  CompositeValidator validator(absl::make_unique<Crc32cHashValidator>(),
                               absl::make_unique<MD5HashValidator>());
  validator.ProcessHeader("x-goog-hash",
                          "crc32c=" + std::string{kQuickFoxCrc32cChecksum});
  auto result = std::move(validator).Finish(
      HashQuick(absl::make_unique<CompositeFunction>(
          absl::make_unique<Crc32cHashFunction>(),
          absl::make_unique<MD5HashFunction>())));
  EXPECT_THAT(result.received.crc32c, kQuickFoxCrc32cChecksum);
  EXPECT_THAT(result.received.md5, IsEmpty());
  EXPECT_FALSE(result.is_mismatch);
}

TEST(HashValidatorImplTest, CreateHashFunctionRead) {
  struct Test {
    std::string crc32c_expected;
    std::string md5_expected;
    DisableCrc32cChecksum crc32_disabled;
    DisableMD5Hash md5_disabled;
  } cases[]{
      {"", "", DisableCrc32cChecksum(true), DisableMD5Hash(true)},
      {"", kQuickFoxMD5Hash, DisableCrc32cChecksum(true),
       DisableMD5Hash(false)},
      {kQuickFoxCrc32cChecksum, "", DisableCrc32cChecksum(false),
       DisableMD5Hash(true)},
      {kQuickFoxCrc32cChecksum, kQuickFoxMD5Hash, DisableCrc32cChecksum(false),
       DisableMD5Hash(false)},
  };

  for (auto const& test : cases) {
    auto request =
        ReadObjectRangeRequest("test-bucket", "test-object")
            .set_multiple_options(test.crc32_disabled, test.md5_disabled);
    auto validator = CreateHashValidator(request);
    auto actual =
        std::move(*validator).Finish(HashQuick(CreateHashFunction(request)));
    EXPECT_EQ(test.crc32c_expected, actual.computed.crc32c);
    EXPECT_EQ(test.md5_expected, actual.computed.md5);
  }
}

TEST(HashValidatorImplTest, CreateHashFunctionUpload) {
  struct Test {
    std::string crc32c_expected;
    std::string md5_expected;
    DisableCrc32cChecksum crc32_disabled;
    Crc32cChecksumValue crc32_value;
    DisableMD5Hash md5_disabled;
    MD5HashValue md5_value;
  } cases[]{
      {"", "", DisableCrc32cChecksum(true), Crc32cChecksumValue(),
       DisableMD5Hash(true), MD5HashValue()},
      {"", "", DisableCrc32cChecksum(true), Crc32cChecksumValue(),
       DisableMD5Hash(true), MD5HashValue(kEmptyStringMD5Hash)},
      {"", kQuickFoxMD5Hash, DisableCrc32cChecksum(true), Crc32cChecksumValue(),
       DisableMD5Hash(false), MD5HashValue()},
      {"", "", DisableCrc32cChecksum(true), Crc32cChecksumValue(),
       DisableMD5Hash(false), MD5HashValue(kEmptyStringMD5Hash)},
      {"", "", DisableCrc32cChecksum(true),
       Crc32cChecksumValue(kEmptyStringCrc32cChecksum), DisableMD5Hash(true),
       MD5HashValue()},
      {"", "", DisableCrc32cChecksum(true),
       Crc32cChecksumValue(kEmptyStringMD5Hash), DisableMD5Hash(true),
       MD5HashValue(kEmptyStringMD5Hash)},
      {"", kQuickFoxMD5Hash, DisableCrc32cChecksum(true),
       Crc32cChecksumValue(kEmptyStringMD5Hash), DisableMD5Hash(false),
       MD5HashValue()},
      {"", "", DisableCrc32cChecksum(true),
       Crc32cChecksumValue(kEmptyStringMD5Hash), DisableMD5Hash(false),
       MD5HashValue(kEmptyStringMD5Hash)},

      {kQuickFoxCrc32cChecksum, "", DisableCrc32cChecksum(false),
       Crc32cChecksumValue(), DisableMD5Hash(true), MD5HashValue()},
      {kQuickFoxCrc32cChecksum, "", DisableCrc32cChecksum(false),
       Crc32cChecksumValue(), DisableMD5Hash(true),
       MD5HashValue(kEmptyStringMD5Hash)},
      {kQuickFoxCrc32cChecksum, kQuickFoxMD5Hash, DisableCrc32cChecksum(false),
       Crc32cChecksumValue(), DisableMD5Hash(false), MD5HashValue()},
      {kQuickFoxCrc32cChecksum, "", DisableCrc32cChecksum(false),
       Crc32cChecksumValue(), DisableMD5Hash(false),
       MD5HashValue(kEmptyStringMD5Hash)},
      {"", "", DisableCrc32cChecksum(false),
       Crc32cChecksumValue(kEmptyStringCrc32cChecksum), DisableMD5Hash(true),
       MD5HashValue()},
      {"", "", DisableCrc32cChecksum(false),
       Crc32cChecksumValue(kEmptyStringCrc32cChecksum), DisableMD5Hash(true),
       MD5HashValue(kEmptyStringMD5Hash)},
      {"", kQuickFoxMD5Hash, DisableCrc32cChecksum(false),
       Crc32cChecksumValue(kEmptyStringCrc32cChecksum), DisableMD5Hash(false),
       MD5HashValue()},
      {"", "", DisableCrc32cChecksum(false),
       Crc32cChecksumValue(kEmptyStringCrc32cChecksum), DisableMD5Hash(false),
       MD5HashValue(kEmptyStringMD5Hash)},
  };

  for (auto const& test : cases) {
    auto request =
        ResumableUploadRequest("test-bucket", "test-object")
            .set_multiple_options(test.crc32_disabled, test.crc32_value,
                                  test.md5_disabled, test.md5_value);
    auto validator = CreateHashValidator(request);
    auto actual =
        std::move(*validator).Finish(HashQuick(CreateHashFunction(request)));
    EXPECT_EQ(test.crc32c_expected, actual.computed.crc32c);
    EXPECT_EQ(test.md5_expected, actual.computed.md5);
  }
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
