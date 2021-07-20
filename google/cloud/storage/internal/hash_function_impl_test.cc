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

#include "google/cloud/storage/internal/hash_function_impl.h"
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

void Update(HashFunction& hash, std::string const& buffer) {
  hash.Update(buffer.data(), buffer.size());
}

TEST(HasFunctionImplTest, EmptyNull) {
  NullHashFunction function;
  auto result = std::move(function).Finish();
  EXPECT_THAT(result.crc32c, IsEmpty());
  EXPECT_THAT(result.md5, IsEmpty());
}

TEST(HasFunctionImplTest, QuickNull) {
  NullHashFunction function;
  Update(function, "The quick");
  Update(function, " brown");
  Update(function, " fox jumps over the lazy dog");
  auto result = std::move(function).Finish();
  EXPECT_THAT(result.crc32c, IsEmpty());
  EXPECT_THAT(result.md5, IsEmpty());
}

TEST(HasFunctionImplTest, EmptyCrc32c) {
  Crc32cHashFunction function;
  auto result = std::move(function).Finish();
  EXPECT_THAT(result.crc32c, kEmptyStringCrc32cChecksum);
  EXPECT_THAT(result.md5, IsEmpty());
}

TEST(HasFunctionImplTest, QuickCrc32c) {
  Crc32cHashFunction function;
  Update(function, "The quick");
  Update(function, " brown");
  Update(function, " fox jumps over the lazy dog");
  auto result = std::move(function).Finish();
  EXPECT_THAT(result.crc32c, kQuickFoxCrc32cChecksum);
  EXPECT_THAT(result.md5, IsEmpty());
}

TEST(HasFunctionImplTest, EmptyMD5) {
  MD5HashFunction function;
  auto result = std::move(function).Finish();
  EXPECT_THAT(result.crc32c, IsEmpty());
  EXPECT_THAT(result.md5, kEmptyStringMD5Hash);
}

TEST(HasFunctionImplTest, QuickMD5) {
  MD5HashFunction function;
  Update(function, "The quick");
  Update(function, " brown");
  Update(function, " fox jumps over the lazy dog");
  auto result = std::move(function).Finish();
  EXPECT_THAT(result.crc32c, IsEmpty());
  EXPECT_THAT(result.md5, kQuickFoxMD5Hash);
}

TEST(HasFunctionImplTest, EmptyComposite) {
  CompositeFunction function(absl::make_unique<MD5HashFunction>(),
                             absl::make_unique<Crc32cHashFunction>());
  auto result = std::move(function).Finish();
  EXPECT_THAT(result.crc32c, kEmptyStringCrc32cChecksum);
  EXPECT_THAT(result.md5, kEmptyStringMD5Hash);
}

TEST(HasFunctionImplTest, QuickComposite) {
  CompositeFunction function(absl::make_unique<MD5HashFunction>(),
                             absl::make_unique<Crc32cHashFunction>());
  Update(function, "The quick");
  Update(function, " brown");
  Update(function, " fox jumps over the lazy dog");
  auto result = std::move(function).Finish();
  EXPECT_THAT(result.crc32c, kQuickFoxCrc32cChecksum);
  EXPECT_THAT(result.md5, kQuickFoxMD5Hash);
}

TEST(HasFunctionImplTest, CreateHashFunctionRead) {
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
    auto function = CreateHashFunction(
        ReadObjectRangeRequest("test-bucket", "test-object")
            .set_multiple_options(test.crc32_disabled, test.md5_disabled));
    Update(*function, "The quick brown fox jumps over the lazy dog");
    auto const actual = std::move(*function).Finish();
    EXPECT_EQ(test.crc32c_expected, actual.crc32c);
    EXPECT_EQ(test.md5_expected, actual.md5);
  }
}

TEST(HasFunctionImplTest, CreateHashFunctionUpload) {
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
    auto function = CreateHashFunction(
        ResumableUploadRequest("test-bucket", "test-object")
            .set_multiple_options(test.crc32_disabled, test.crc32_value,
                                  test.md5_disabled, test.md5_value));
    Update(*function, "The quick brown fox jumps over the lazy dog");
    auto const actual = std::move(*function).Finish();
    EXPECT_EQ(test.crc32c_expected, actual.crc32c);
    EXPECT_EQ(test.md5_expected, actual.md5);
  }
}

TEST(HasFunctionImplTest, CreateHashFunctionUploadResumedSession) {
  auto function = CreateHashFunction(
      ResumableUploadRequest("test-bucket", "test-object")
          .set_multiple_options(UseResumableUploadSession("test-session-id"),
                                DisableCrc32cChecksum(false),
                                DisableMD5Hash(false)));
  Update(*function, "The quick brown fox jumps over the lazy dog");
  auto const actual = std::move(*function).Finish();
  EXPECT_THAT(actual.crc32c, IsEmpty());
  EXPECT_THAT(actual.crc32c, IsEmpty());
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
