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
#include "google/cloud/storage/internal/hash_function.h"
#include "google/cloud/storage/internal/hash_values.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage_experimental::ReadPayload;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Optional;
using ::testing::ResultOf;
using ::testing::VariantWith;

class MockHashFunction : public storage::internal::HashFunction {
 public:
  MOCK_METHOD(void, Update, (absl::string_view buffer), (override));
  MOCK_METHOD(Status, Update, (std::int64_t offset, absl::string_view buffer),
              (override));
  MOCK_METHOD(Status, Update,
              (std::int64_t offset, absl::string_view buffer,
               std::uint32_t buffer_crc),
              (override));
  MOCK_METHOD(Status, Update,
              (std::int64_t offset, absl::Cord const& buffer,
               std::uint32_t buffer_crc),
              (override));
  MOCK_METHOD(std::string, Name, (), (const, override));
  MOCK_METHOD(storage::internal::HashValues, Finish, (), (override));
};

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
  absl::string_view contents("1234567890");
  EXPECT_CALL(*hash_function, Update(0, contents, _)).Times(AtLeast(1));

  ReadRange actual(0, 0, hash_function);
  auto data = google::storage::v2::ObjectRangeData{};
  auto constexpr kData0 = R"pb(
    checksummed_data { content: "1234567890" }
    read_range { read_offset: 0 read_length: 10 read_id: 7 }
    range_end: false
  )pb";

  EXPECT_TRUE(TextFormat::ParseFromString(kData0, &data));
  actual.OnRead(std::move(data));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
