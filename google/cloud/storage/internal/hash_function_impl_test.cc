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
#include "google/cloud/storage/testing/mock_hash_function.h"
#include "google/cloud/storage/testing/upload_hash_cases.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::storage::testing::MockHashFunction;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::An;
using ::testing::IsEmpty;
using ::testing::Return;

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
  auto function = MD5HashFunction::Create();
  auto result = std::move(function)->Finish();
  EXPECT_THAT(result.crc32c, IsEmpty());
  EXPECT_THAT(result.md5, kEmptyStringMD5Hash);
}

TEST(HashFunctionImplTest, MD5Quick) {
  auto function = MD5HashFunction::Create();
  function->Update("The quick");
  function->Update(" brown");
  function->Update(" fox jumps over the lazy dog");
  auto result = std::move(function)->Finish();
  EXPECT_THAT(result.crc32c, IsEmpty());
  EXPECT_THAT(result.md5, kQuickFoxMD5Hash);
}

TEST(HashFunctionImplTest, MD5StringView) {
  auto function = MD5HashFunction::Create();
  auto payload = absl::string_view{kQuickFox};
  for (std::size_t pos = 0; pos < payload.size(); pos += 5) {
    auto message = payload.substr(pos, 5);
    EXPECT_STATUS_OK(function->Update(pos, message));
    EXPECT_STATUS_OK(function->Update(pos, message));
    EXPECT_THAT(function->Update(pos, payload),
                StatusIs(StatusCode::kInvalidArgument));
  }
  auto actual = function->Finish();
  EXPECT_THAT(actual.crc32c, IsEmpty());
  EXPECT_THAT(actual.md5, kQuickFoxMD5Hash);

  actual = function->Finish();
  EXPECT_THAT(actual.crc32c, IsEmpty());
  EXPECT_THAT(actual.md5, kQuickFoxMD5Hash);
}

TEST(HashFunctionImplTest, MD5StringViewWithCrc) {
  auto function = MD5HashFunction::Create();
  auto payload = absl::string_view{kQuickFox};
  for (std::size_t pos = 0; pos < payload.size(); pos += 5) {
    auto message = payload.substr(pos, 5);
    auto const unused = std::uint32_t{0};
    EXPECT_STATUS_OK(function->Update(pos, message, unused));
    EXPECT_STATUS_OK(function->Update(pos, message, unused));
    EXPECT_THAT(function->Update(pos, payload, unused),
                StatusIs(StatusCode::kInvalidArgument));
  }
  auto actual = function->Finish();
  EXPECT_THAT(actual.crc32c, IsEmpty());
  EXPECT_THAT(actual.md5, kQuickFoxMD5Hash);

  actual = function->Finish();
  EXPECT_THAT(actual.crc32c, IsEmpty());
  EXPECT_THAT(actual.md5, kQuickFoxMD5Hash);
}

TEST(HashFunctionImplTest, MD5Cord) {
  auto function = MD5HashFunction::Create();
  auto payload = absl::Cord(absl::string_view{kQuickFox});
  for (std::size_t pos = 0; pos < payload.size(); pos += 5) {
    auto message = payload.Subcord(pos, 5);
    auto const unused = std::uint32_t{0};
    EXPECT_STATUS_OK(function->Update(pos, message, unused));
    EXPECT_STATUS_OK(function->Update(pos, message, unused));
    EXPECT_THAT(function->Update(pos, payload, unused),
                StatusIs(StatusCode::kInvalidArgument));
  }
  auto const actual = function->Finish();
  EXPECT_THAT(actual.crc32c, IsEmpty());
  EXPECT_THAT(actual.md5, kQuickFoxMD5Hash);

  auto const a2 = function->Finish();
  EXPECT_THAT(a2.crc32c, IsEmpty());
  EXPECT_THAT(a2.md5, kQuickFoxMD5Hash);
}

TEST(HashFunctionImplTest, CompositeEmpty) {
  CompositeFunction function(MD5HashFunction::Create(),
                             std::make_unique<Crc32cHashFunction>());
  auto result = std::move(function).Finish();
  EXPECT_THAT(result.crc32c, kEmptyStringCrc32cChecksum);
  EXPECT_THAT(result.md5, kEmptyStringMD5Hash);
}

TEST(HashFunctionImplTest, CompositeQuick) {
  CompositeFunction function(MD5HashFunction::Create(),
                             std::make_unique<Crc32cHashFunction>());
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

TEST(HashFunctionImplTest, PrecomputedQuick) {
  PrecomputedHashFunction function{
      HashValues{kEmptyStringCrc32cChecksum, kEmptyStringMD5Hash}};
  function.Update("The quick");
  function.Update(" brown");
  function.Update(" fox jumps over the lazy dog");
  auto const actual = std::move(function).Finish();
  EXPECT_THAT(actual.crc32c, kEmptyStringCrc32cChecksum);
  EXPECT_THAT(actual.md5, kEmptyStringMD5Hash);
}

TEST(HashFunctionImplTest, Crc32cMessageDelegation) {
  auto expected_status = [](std::string m) {
    return Status(StatusCode::kUnknown, std::move(m));
  };

  auto mock = std::make_unique<MockHashFunction>();
  EXPECT_CALL(*mock, Name).WillOnce(Return("mock"));
  EXPECT_CALL(*mock, Update(An<absl::string_view>())).Times(1);
  EXPECT_CALL(*mock, Update(_, An<absl::string_view>()))
      .WillOnce(Return(expected_status("Update(_, absl::string_view)")));
  EXPECT_CALL(*mock, Update(_, An<absl::string_view>(), _))
      .WillOnce(Return(expected_status("Update(_, absl::string_view, _)")));
  EXPECT_CALL(*mock, Update(_, An<absl::Cord const&>(), _))
      .WillOnce(Return(expected_status("Update(_, absl::Cord const&, _)")));
  EXPECT_CALL(*mock, Finish)
      .WillOnce(Return(HashValues{"finish-crc32c", "finish-md5"}));

  Crc32cMessageHashFunction function(std::move(mock));
  EXPECT_EQ(function.Name(), "crc32c-message(mock)");
  EXPECT_NO_FATAL_FAILURE(function.Update(absl::string_view("unused")));
  EXPECT_THAT(function.Update(0, absl::string_view("")),
              StatusIs(StatusCode::kUnknown, "Update(_, absl::string_view)"));
  EXPECT_THAT(
      function.Update(0, absl::string_view(""), 0),
      StatusIs(StatusCode::kUnknown, "Update(_, absl::string_view, _)"));
  EXPECT_THAT(
      function.Update(0, absl::Cord(), 0),
      StatusIs(StatusCode::kUnknown, "Update(_, absl::Cord const&, _)"));
  auto const values = function.Finish();
  EXPECT_EQ(values.crc32c, "finish-crc32c");
  EXPECT_EQ(values.md5, "finish-md5");
}

TEST(HashFunctionImplTest, Crc32cMessageValidate) {
  auto const message =
      absl::string_view{"The quick brown fox jumps over the lazy dog"};
  auto const expected_crc32c = storage_internal::Crc32c(message);
  auto mock = std::make_unique<MockHashFunction>();
  EXPECT_CALL(*mock, Update(_, message, expected_crc32c))
      .WillRepeatedly(Return(Status{}));
  EXPECT_CALL(*mock, Update(_, absl::Cord(message), expected_crc32c))
      .WillRepeatedly(Return(Status{}));

  Crc32cMessageHashFunction function(std::move(mock));
  EXPECT_THAT(function.Update(0, message, expected_crc32c), IsOk());
  EXPECT_THAT(function.Update(0, absl::Cord(message), expected_crc32c), IsOk());

  EXPECT_THAT(function.Update(0, message, expected_crc32c + 1),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(function.Update(0, absl::Cord(message), expected_crc32c + 1),
              StatusIs(StatusCode::kInvalidArgument));
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

struct UploadTest {
  std::string crc32c_expected;
  std::string md5_expected;
  DisableCrc32cChecksum crc32_disabled;
  Crc32cChecksumValue crc32_value;
  DisableMD5Hash md5_disabled;
  MD5HashValue md5_value;
};

TEST(HashFunctionImplTest, CreateHashFunctionUpload) {
  auto const upload_cases = testing::UploadHashCases();

  for (auto const& test : upload_cases) {
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
  auto const upload_cases = testing::UploadHashCases();

  for (auto const& test : upload_cases) {
    auto function = CreateHashFunction(test.crc32_value, test.crc32_disabled,
                                       test.md5_value, test.md5_disabled);
    ASSERT_STATUS_OK(function->Update(/*offset=*/0, kQuickFox));
    auto const actual = function->Finish();
    EXPECT_EQ(test.crc32c_expected, actual.crc32c);
    EXPECT_EQ(test.md5_expected, actual.md5);
  }
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
