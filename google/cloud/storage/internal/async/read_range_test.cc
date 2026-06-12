// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async/read_range.h"
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/storage/internal/hash_function.h"
#include "google/cloud/storage/internal/hash_function_impl.h"
#include "google/cloud/storage/internal/hash_validator_impl.h"
#include "google/cloud/storage/internal/hash_values.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_hash_function.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <type_traits>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::ReadPayload;
using ::google::cloud::storage::testing::MockHashFunction;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::An;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Optional;
using ::testing::ResultOf;
using ::testing::VariantWith;

using HashUpdateType =
    std::conditional_t<std::is_same<absl::Cord, ContentType>::value,
                       absl::Cord const&, absl::string_view>;

TEST(ReadRange, BasicLifecycle) {
  ReadRange actual(10000, 40);
  EXPECT_FALSE(actual.IsDone());
  auto range = google::storage::v2::ReadRange{};
  auto constexpr kRange0 = R"pb(
    read_id: 7 read_offset: 10000 read_length: 40
  )pb";
  EXPECT_TRUE(TextFormat::ParseFromString(kRange0, &range));
  EXPECT_THAT(actual.RangeForResume(7), Optional(IsProtoEqual(range)));

  auto pending = actual.Read();
  EXPECT_FALSE(pending.is_ready());

  auto data = google::storage::v2::ObjectRangeData{};
  auto constexpr kData0 = R"pb(
    checksummed_data { content: "0123456789" }
    read_range { read_offset: 10000 read_length: 10 read_id: 7 }
    range_end: false
  )pb";
  EXPECT_TRUE(TextFormat::ParseFromString(kData0, &data));
  actual.OnRead(std::move(data));

  EXPECT_TRUE(pending.is_ready());
  EXPECT_THAT(pending.get(),
              VariantWith<ReadPayload>(ResultOf(
                  "contents", [](ReadPayload const& p) { return p.contents(); },
                  ElementsAre("0123456789"))));
  range = google::storage::v2::ReadRange{};
  auto constexpr kRange1 = R"pb(
    read_id: 7 read_offset: 10010 read_length: 30
  )pb";
  EXPECT_TRUE(TextFormat::ParseFromString(kRange1, &range));
  EXPECT_THAT(actual.RangeForResume(7), Optional(IsProtoEqual(range)));

  auto constexpr kData1 = R"pb(
    checksummed_data { content: "1234567890" }
    read_range { read_offset: 10020 read_length: 10 read_id: 7 }
    range_end: false
  )pb";
  EXPECT_TRUE(TextFormat::ParseFromString(kData1, &data));
  actual.OnRead(std::move(data));

  pending = actual.Read();
  EXPECT_TRUE(pending.is_ready());
  EXPECT_THAT(pending.get(),
              VariantWith<ReadPayload>(ResultOf(
                  "contents", [](ReadPayload const& p) { return p.contents(); },
                  ElementsAre("1234567890"))));

  data = google::storage::v2::ObjectRangeData{};
  auto constexpr kData2 = R"pb(
    checksummed_data { content: "2345678901" }
    read_range { read_offset: 10030 read_length: 10 read_id: 7 }
    range_end: true
  )pb";
  EXPECT_TRUE(TextFormat::ParseFromString(kData2, &data));
  actual.OnRead(std::move(data));

  EXPECT_TRUE(actual.IsDone());
  EXPECT_FALSE(actual.RangeForResume(7).has_value());

  pending = actual.Read();
  EXPECT_TRUE(pending.is_ready());
  EXPECT_THAT(pending.get(),
              VariantWith<ReadPayload>(ResultOf(
                  "contents", [](ReadPayload const& p) { return p.contents(); },
                  ElementsAre("2345678901"))));

  actual.OnFinish(Status{});
  EXPECT_THAT(actual.Read().get(), VariantWith<Status>(IsOk()));
  // A second Read() should be harmless.
  EXPECT_THAT(actual.Read().get(), VariantWith<Status>(IsOk()));
}

TEST(ReadRange, Error) {
  ReadRange actual(10000, 40);
  auto pending = actual.Read();
  EXPECT_FALSE(pending.is_ready());
  actual.OnFinish(PermanentError());

  EXPECT_THAT(pending.get(),
              VariantWith<Status>(StatusIs(PermanentError().code())));
}

TEST(ReadRange, Queue) {
  ReadRange actual(10000, 40);

  auto data = google::storage::v2::ObjectRangeData{};
  auto constexpr kData0 = R"pb(
    checksummed_data { content: "0123456789" }
    read_range { read_offset: 10000 read_length: 10 read_id: 7 }
    range_end: false
  )pb";
  EXPECT_TRUE(TextFormat::ParseFromString(kData0, &data));
  actual.OnRead(std::move(data));

  auto constexpr kData1 = R"pb(
    checksummed_data { content: "1234567890" }
    read_range { read_offset: 10020 read_length: 10 read_id: 7 }
    range_end: false
  )pb";
  EXPECT_TRUE(TextFormat::ParseFromString(kData1, &data));
  actual.OnRead(std::move(data));

  auto matcher = ResultOf(
      "contents",
      [](ReadPayload const& p) {
        // For small strings, absl::Cord may choose to merge the strings into a
        // single value. Testing with `ElementsAre()` won't work in all
        // environments.
        std::string m;
        for (auto sv : p.contents()) {
          m += std::string(sv);
        }
        return m;
      },
      "01234567891234567890");
  EXPECT_THAT(actual.Read().get(), VariantWith<ReadPayload>(matcher));
}

TEST(ReadRange, HashFunctionCalled) {
  auto hash_function = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash_function, Update(0, An<HashUpdateType>(), _))
      .Times(AtLeast(1));

  ReadRange actual(0, 10, hash_function);
  auto data = google::storage::v2::ObjectRangeData{};
  auto constexpr kData0 = R"pb(
    checksummed_data { content: "1234567890" }
    read_range { read_offset: 0 read_length: 10 read_id: 7 }
    range_end: false
  )pb";

  EXPECT_TRUE(TextFormat::ParseFromString(kData0, &data));
  actual.OnRead(std::move(data));
}

TEST(ReadRange, FullObjectChecksumValidationMismatch) {
  auto hash_function =
      std::make_shared<storage::internal::Crc32cHashFunction>();
  auto hash_validator =
      std::make_unique<storage::internal::Crc32cHashValidator>();

  storage::internal::HashValues expected_hashes;
  expected_hashes.crc32c = "n3tPLw==";  // Some expected hash
  hash_validator->ProcessHashValues(expected_hashes);

  ReadRange actual(0, 10, hash_function, std::move(hash_validator));

  auto data = google::storage::v2::ObjectRangeData{};
  auto constexpr kData0 = R"pb(
    checksummed_data { content: "1234567890" }
    read_range { read_offset: 0 read_length: 10 read_id: 7 }
    range_end: true
  )pb";

  EXPECT_TRUE(TextFormat::ParseFromString(kData0, &data));

  auto pending = actual.Read();
  actual.OnRead(std::move(data));

  EXPECT_TRUE(actual.IsDone());

  // First read gets the data
  EXPECT_TRUE(pending.is_ready());
  EXPECT_THAT(pending.get(), VariantWith<ReadPayload>(_));

  // Second read gets the error
  auto next_read = actual.Read();
  EXPECT_TRUE(next_read.is_ready());
  EXPECT_THAT(next_read.get(),
              VariantWith<Status>(StatusIs(StatusCode::kDataLoss)));
}

TEST(ReadRange, FullObjectChecksumValidationMismatchMD5) {
  auto hash_function = std::shared_ptr<storage::internal::HashFunction>(
      storage::internal::MD5HashFunction::Create());
  auto hash_validator = std::make_unique<storage::internal::MD5HashValidator>();

  storage::internal::HashValues expected_hashes;
  expected_hashes.md5 = "wrong-md5-hash";
  hash_validator->ProcessHashValues(expected_hashes);

  ReadRange actual(0, 43, std::move(hash_function), std::move(hash_validator));

  auto data = google::storage::v2::ObjectRangeData{};
  auto constexpr kData0 = R"pb(
    checksummed_data {
      content: "The quick brown fox jumps over the lazy dog"
      crc32c: 576848900
    }
    read_range { read_offset: 0 read_length: 43 read_id: 7 }
    range_end: true
  )pb";

  EXPECT_TRUE(TextFormat::ParseFromString(kData0, &data));

  auto pending = actual.Read();
  actual.OnRead(std::move(data));

  EXPECT_TRUE(actual.IsDone());

  // First read gets the data
  EXPECT_TRUE(pending.is_ready());
  EXPECT_THAT(pending.get(), VariantWith<ReadPayload>(_));

  // Second read gets the error
  auto next_read = actual.Read();
  EXPECT_TRUE(next_read.is_ready());
  EXPECT_THAT(next_read.get(),
              VariantWith<Status>(StatusIs(StatusCode::kDataLoss)));
}

TEST(ReadRange, FullObjectChecksumValidationMismatchComposite) {
  auto hash_function = std::make_shared<storage::internal::CompositeFunction>(
      std::make_unique<storage::internal::Crc32cHashFunction>(),
      storage::internal::MD5HashFunction::Create());
  auto hash_validator = std::make_unique<storage::internal::CompositeValidator>(
      std::make_unique<storage::internal::Crc32cHashValidator>(),
      std::make_unique<storage::internal::MD5HashValidator>());

  storage::internal::HashValues expected_hashes;
  expected_hashes.crc32c = "wrong-crc32c";
  expected_hashes.md5 = "wrong-md5";
  hash_validator->ProcessHashValues(expected_hashes);

  ReadRange actual(0, 43, hash_function, std::move(hash_validator));

  auto data = google::storage::v2::ObjectRangeData{};
  auto constexpr kData0 = R"pb(
    checksummed_data {
      content: "The quick brown fox jumps over the lazy dog"
      crc32c: 576848900
    }
    read_range { read_offset: 0 read_length: 43 read_id: 7 }
    range_end: true
  )pb";

  EXPECT_TRUE(TextFormat::ParseFromString(kData0, &data));

  auto pending = actual.Read();
  actual.OnRead(std::move(data));

  EXPECT_TRUE(actual.IsDone());

  // First read gets the data
  EXPECT_TRUE(pending.is_ready());
  EXPECT_THAT(pending.get(), VariantWith<ReadPayload>(_));

  // Second read gets the error
  auto next_read = actual.Read();
  EXPECT_TRUE(next_read.is_ready());
  EXPECT_THAT(next_read.get(),
              VariantWith<Status>(StatusIs(StatusCode::kDataLoss)));
}

TEST(ReadRange, FullObjectChecksumValidationSuccessComposite) {
  auto hash_function = std::make_shared<storage::internal::CompositeFunction>(
      std::make_unique<storage::internal::Crc32cHashFunction>(),
      storage::internal::MD5HashFunction::Create());
  auto hash_validator = std::make_unique<storage::internal::CompositeValidator>(
      std::make_unique<storage::internal::Crc32cHashValidator>(),
      std::make_unique<storage::internal::MD5HashValidator>());

  storage::internal::HashValues expected_hashes;
  // CRC32C of "The quick brown fox jumps over the lazy dog" is "ImIEBA=="
  // MD5 of "The quick brown fox jumps over the lazy dog" is
  // "nhB9nTcrtoJr2B01QqQZ1g=="
  expected_hashes.crc32c = "ImIEBA==";
  expected_hashes.md5 = "nhB9nTcrtoJr2B01QqQZ1g==";
  hash_validator->ProcessHashValues(expected_hashes);

  ReadRange actual(0, 43, hash_function, std::move(hash_validator));

  auto data = google::storage::v2::ObjectRangeData{};
  auto constexpr kData0 = R"pb(
    checksummed_data {
      content: "The quick brown fox jumps over the lazy dog"
      crc32c: 576848900
    }
    read_range { read_offset: 0 read_length: 43 read_id: 7 }
    range_end: true
  )pb";

  EXPECT_TRUE(TextFormat::ParseFromString(kData0, &data));

  auto pending = actual.Read();
  actual.OnRead(std::move(data));

  EXPECT_TRUE(actual.IsDone());

  // First read gets the data
  EXPECT_TRUE(pending.is_ready());
  EXPECT_THAT(pending.get(), VariantWith<ReadPayload>(_));

  // Second read gets OK status instead of DataLoss
  auto next_read = actual.Read();
  EXPECT_TRUE(next_read.is_ready());
  EXPECT_THAT(next_read.get(), VariantWith<Status>(IsOk()));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
