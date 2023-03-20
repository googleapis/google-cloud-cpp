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

#include "google/cloud/storage/internal/hash_function_impl.h"
#include "google/cloud/storage/internal/crc32c.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::IsEmpty;

// These values were obtained using:
// echo -n '' > foo.txt && gsutil hash foo.txt
auto constexpr kEmptyStringCrc32cChecksum = "AAAAAA==";
auto constexpr kEmptyStringMD5Hash = "1B2M2Y8AsgTpgAmY7PhCfg==";

// /bin/echo -n 'The quick brown fox jumps over the lazy dog' > foo.txt
// gsutil hash foo.txt
auto constexpr kQuickFoxCrc32cChecksum = "ImIEBA==";
auto constexpr kQuickFoxMD5Hash = "nhB9nTcrtoJr2B01QqQZ1g==";
auto constexpr kQuickFox = "The quick brown fox jumps over the lazy dog";

TEST(HashFunctionImplTest, EmptyNull) {
  NullHashFunction function;
  auto result = std::move(function).Finish();
  EXPECT_THAT(result.crc32c, IsEmpty());
  EXPECT_THAT(result.md5, IsEmpty());
}

TEST(HashFunctionImplTest, QuickNull) {
  NullHashFunction function;
  function.Update("The quick");
  function.Update(" brown");
  function.Update(" fox jumps over the lazy dog");
  auto result = std::move(function).Finish();
  EXPECT_THAT(result.crc32c, IsEmpty());
  EXPECT_THAT(result.md5, IsEmpty());
}

TEST(HashFunctionImplTest, Crc32cEmpty) {
  Crc32cHashFunction function;
  auto result = std::move(function).Finish();
  EXPECT_THAT(result.crc32c, kEmptyStringCrc32cChecksum);
  EXPECT_THAT(result.md5, IsEmpty());
}

TEST(HashFunctionImplTest, Crc32cQuickSimple) {
  Crc32cHashFunction function;
  function.Update("The quick");
  function.Update(" brown");
  function.Update(" fox jumps over the lazy dog");
  auto result = std::move(function).Finish();
  EXPECT_THAT(result.crc32c, kQuickFoxCrc32cChecksum);
  EXPECT_THAT(result.md5, IsEmpty());
}

TEST(HashFunctionImplTest, Crc32cStringView) {
  Crc32cHashFunction function;
  auto payload = absl::string_view{kQuickFox};
  for (std::size_t pos = 0; pos < payload.size(); pos += 5) {
    auto message = payload.substr(pos, 5);
    EXPECT_STATUS_OK(function.Update(pos, message));
    EXPECT_STATUS_OK(function.Update(pos, message));
    EXPECT_THAT(function.Update(pos, payload),
                StatusIs(StatusCode::kInvalidArgument));
  }
  auto actual = function.Finish();
  EXPECT_EQ(actual.crc32c, kQuickFoxCrc32cChecksum);
  EXPECT_THAT(actual.md5, IsEmpty());

  actual = function.Finish();
  EXPECT_EQ(actual.crc32c, kQuickFoxCrc32cChecksum);
  EXPECT_THAT(actual.md5, IsEmpty());
}

TEST(HashFunctionImplTest, Crc32cStringViewWithCrc) {
  Crc32cHashFunction function;
  auto payload = absl::string_view{kQuickFox};
  for (std::size_t pos = 0; pos < payload.size(); pos += 5) {
    auto message = payload.substr(pos, 5);
    auto const message_crc = storage_internal::Crc32c(message);
    EXPECT_STATUS_OK(function.Update(pos, message, message_crc));
    EXPECT_STATUS_OK(function.Update(pos, message, message_crc));
    EXPECT_THAT(function.Update(pos, payload, message_crc),
                StatusIs(StatusCode::kInvalidArgument));
  }
  auto actual = function.Finish();
  EXPECT_EQ(actual.crc32c, kQuickFoxCrc32cChecksum);
  EXPECT_THAT(actual.md5, IsEmpty());

  actual = function.Finish();
  EXPECT_EQ(actual.crc32c, kQuickFoxCrc32cChecksum);
  EXPECT_THAT(actual.md5, IsEmpty());
}

TEST(HashFunctionImplTest, Crc32cCord) {
  Crc32cHashFunction function;
  auto payload = absl::Cord(absl::string_view{kQuickFox});
  for (std::size_t pos = 0; pos < payload.size(); pos += 5) {
    auto message = payload.Subcord(pos, 5);
    auto const message_crc = storage_internal::Crc32c(message);
    EXPECT_STATUS_OK(function.Update(pos, message, message_crc));
    EXPECT_STATUS_OK(function.Update(pos, message, message_crc));
    EXPECT_THAT(function.Update(pos, payload, message_crc),
                StatusIs(StatusCode::kInvalidArgument));
  }
  auto actual = function.Finish();
  EXPECT_EQ(actual.crc32c, kQuickFoxCrc32cChecksum);
  EXPECT_THAT(actual.md5, IsEmpty());

  actual = function.Finish();
  EXPECT_EQ(actual.crc32c, kQuickFoxCrc32cChecksum);
  EXPECT_THAT(actual.md5, IsEmpty());
}

TEST(HashFunctionImplTest, MD5Empty) {
  MD5HashFunction function;
  auto result = std::move(function).Finish();
  EXPECT_THAT(result.crc32c, IsEmpty());
  EXPECT_THAT(result.md5, kEmptyStringMD5Hash);
}

TEST(HashFunctionImplTest, MD5Quick) {
  MD5HashFunction function;
  function.Update("The quick");
  function.Update(" brown");
  function.Update(" fox jumps over the lazy dog");
  auto result = std::move(function).Finish();
  EXPECT_THAT(result.crc32c, IsEmpty());
  EXPECT_THAT(result.md5, kQuickFoxMD5Hash);
}

TEST(HashFunctionImplTest, MD5StringView) {
  MD5HashFunction function;
  auto payload = absl::string_view{kQuickFox};
  for (std::size_t pos = 0; pos < payload.size(); pos += 5) {
    auto message = payload.substr(pos, 5);
    EXPECT_STATUS_OK(function.Update(pos, message));
    EXPECT_STATUS_OK(function.Update(pos, message));
    EXPECT_THAT(function.Update(pos, payload),
                StatusIs(StatusCode::kInvalidArgument));
  }
  auto actual = function.Finish();
  EXPECT_THAT(actual.crc32c, IsEmpty());
  EXPECT_THAT(actual.md5, kQuickFoxMD5Hash);

  actual = function.Finish();
  EXPECT_THAT(actual.crc32c, IsEmpty());
  EXPECT_THAT(actual.md5, kQuickFoxMD5Hash);
}

TEST(HashFunctionImplTest, MD5StringViewWithCrc) {
  MD5HashFunction function;
  auto payload = absl::string_view{kQuickFox};
  for (std::size_t pos = 0; pos < payload.size(); pos += 5) {
    auto message = payload.substr(pos, 5);
    auto const unused = std::uint32_t{0};
    EXPECT_STATUS_OK(function.Update(pos, message, unused));
    EXPECT_STATUS_OK(function.Update(pos, message, unused));
    EXPECT_THAT(function.Update(pos, payload, unused),
                StatusIs(StatusCode::kInvalidArgument));
  }
  auto actual = function.Finish();
  EXPECT_THAT(actual.crc32c, IsEmpty());
  EXPECT_THAT(actual.md5, kQuickFoxMD5Hash);

  actual = function.Finish();
  EXPECT_THAT(actual.crc32c, IsEmpty());
  EXPECT_THAT(actual.md5, kQuickFoxMD5Hash);
}

TEST(HashFunctionImplTest, MD5Cord) {
  MD5HashFunction function;
  auto payload = absl::Cord(absl::string_view{kQuickFox});
  for (std::size_t pos = 0; pos < payload.size(); pos += 5) {
    auto message = payload.Subcord(pos, 5);
    auto const unused = std::uint32_t{0};
    EXPECT_STATUS_OK(function.Update(pos, message, unused));
    EXPECT_STATUS_OK(function.Update(pos, message, unused));
    EXPECT_THAT(function.Update(pos, payload, unused),
                StatusIs(StatusCode::kInvalidArgument));
  }
  auto const actual = function.Finish();
  EXPECT_THAT(actual.crc32c, IsEmpty());
  EXPECT_THAT(actual.md5, kQuickFoxMD5Hash);

  auto const a2 = function.Finish();
  EXPECT_THAT(a2.crc32c, IsEmpty());
  EXPECT_THAT(a2.md5, kQuickFoxMD5Hash);
}

TEST(HashFunctionImplTest, CompositeEmpty) {
  CompositeFunction function(absl::make_unique<MD5HashFunction>(),
                             absl::make_unique<Crc32cHashFunction>());
  auto result = std::move(function).Finish();
  EXPECT_THAT(result.crc32c, kEmptyStringCrc32cChecksum);
  EXPECT_THAT(result.md5, kEmptyStringMD5Hash);
}

TEST(HashFunctionImplTest, CompositeQuick) {
  CompositeFunction function(absl::make_unique<MD5HashFunction>(),
                             absl::make_unique<Crc32cHashFunction>());
  function.Update("The quick");
  function.Update(" brown");
  function.Update(" fox jumps over the lazy dog");
  auto actual = function.Finish();
  EXPECT_THAT(actual.crc32c, kQuickFoxCrc32cChecksum);
  EXPECT_THAT(actual.md5, kQuickFoxMD5Hash);

  actual = function.Finish();
  EXPECT_THAT(actual.crc32c, kQuickFoxCrc32cChecksum);
  EXPECT_THAT(actual.md5, kQuickFoxMD5Hash);
}

TEST(HashFunctionImplTest, CreateHashFunctionRead) {
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
    function->Update(kQuickFox);
    auto const actual = std::move(*function).Finish();
    EXPECT_EQ(test.crc32c_expected, actual.crc32c);
    EXPECT_EQ(test.md5_expected, actual.md5);
  }
}

TEST(HashFunctionImplTest, CreateHashFunctionUpload) {
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
    function->Update(kQuickFox);
    auto const actual = std::move(*function).Finish();
    EXPECT_EQ(test.crc32c_expected, actual.crc32c);
    EXPECT_EQ(test.md5_expected, actual.md5);
  }
}

TEST(HashFunctionImplTest, CreateHashFunctionUploadResumedSession) {
  auto function = CreateHashFunction(
      ResumableUploadRequest("test-bucket", "test-object")
          .set_multiple_options(UseResumableUploadSession("test-session-id"),
                                DisableCrc32cChecksum(false),
                                DisableMD5Hash(false)));
  function->Update(kQuickFox);
  auto const actual = std::move(*function).Finish();
  EXPECT_THAT(actual.crc32c, IsEmpty());
  EXPECT_THAT(actual.md5, IsEmpty());
}

TEST(HashFunctionImplTest, CreateHashFunctionInsertObjectMedia) {
  struct Test {
    DisableCrc32cChecksum crc32c;
    DisableMD5Hash md5;
    HashValues expected;
  } const cases[] = {
      {DisableCrc32cChecksum(false), DisableMD5Hash(false),
       HashValues{kQuickFoxCrc32cChecksum, kQuickFoxMD5Hash}},
      {DisableCrc32cChecksum(false), DisableMD5Hash(true),
       HashValues{kQuickFoxCrc32cChecksum, {}}},
      {DisableCrc32cChecksum(true), DisableMD5Hash(false),
       HashValues{{}, kQuickFoxMD5Hash}},
      {DisableCrc32cChecksum(true), DisableMD5Hash(true), HashValues{{}, {}}},
  };
  for (auto const& test : cases) {
    auto function = CreateHashFunction(
        InsertObjectMediaRequest("test-bucket", "test-object", kQuickFox)
            .set_multiple_options(test.crc32c, test.md5));
    ASSERT_STATUS_OK(function->Update(/*offset=*/0, kQuickFox));
    auto const actual = function->Finish();
    EXPECT_EQ(actual.crc32c, test.expected.crc32c);
    EXPECT_EQ(actual.md5, test.expected.md5);
  }
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
