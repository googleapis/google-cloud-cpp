// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/metadata_parser.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <limits>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::StatusIs;

/// @test Verify that we parse boolean values in JSON objects.
TEST(MetadataParserTest, ParseBoolField) {
  std::string text = R"""({
      "flag1": true,
      "flag2": false
})""";
  auto json_object = nlohmann::json::parse(text);
  EXPECT_TRUE(ParseBoolField(json_object, "flag1").value());
  EXPECT_FALSE(ParseBoolField(json_object, "flag2").value());
}

/// @test Verify that we parse boolean values in JSON objects.
TEST(MetadataParserTest, ParseBoolFieldFromString) {
  std::string text = R"""({
      "flag1": "true",
      "flag2": "false"
})""";
  auto json_object = nlohmann::json::parse(text);
  EXPECT_TRUE(ParseBoolField(json_object, "flag1").value());
  EXPECT_FALSE(ParseBoolField(json_object, "flag2").value());
}

/// @test Verify that we parse missing boolean values in JSON objects.
TEST(MetadataParserTest, ParseMissingBoolField) {
  std::string text = R"""({
      "flag": true
})""";
  auto json_object = nlohmann::json::parse(text);
  auto actual = ParseBoolField(json_object, "some-other-flag").value();
  EXPECT_FALSE(actual);
}

/// @test Verify that we raise an exception with invalid boolean fields.
TEST(MetadataParserTest, ParseInvalidBoolFieldValue) {
  std::string text = R"""({"flag": "not-a-boolean"})""";
  auto json_object = nlohmann::json::parse(text);

  EXPECT_THAT(ParseBoolField(json_object, "flag"),
              StatusIs(StatusCode::kInvalidArgument));
}

/// @test Verify that we raise an exception with invalid boolean fields.
TEST(MetadataParserTest, ParseInvalidBoolFieldType) {
  std::string text = R"""({
      "flag": [0, 1, 2]
})""";
  auto json_object = nlohmann::json::parse(text);

  EXPECT_THAT(ParseBoolField(json_object, "flag"),
              StatusIs(StatusCode::kInvalidArgument));
}

/// @test Verify that we parse RFC-3339 timestamps in JSON objects.
TEST(MetadataParserTest, ParseTimestampField) {
  std::string text = R"""({
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z"
})""";
  auto json_object = nlohmann::json::parse(text);
  auto actual = ParseTimestampField(json_object, "timeCreated");
  ASSERT_STATUS_OK(actual);

  // Use `date -u +%s --date='2018-05-19T19:31:14Z'` to get the magic number:
  using std::chrono::duration_cast;
  EXPECT_EQ(
      1526758274L,
      duration_cast<std::chrono::seconds>(actual->time_since_epoch()).count());
}

/// @test Verify that we parse RFC-3339 timestamps in JSON objects.
TEST(MetadataParserTest, ParseMissingTimestampField) {
  std::string text = R"""({
      "updated": "2018-05-19T19:31:24Z"
})""";
  auto json_object = nlohmann::json::parse(text);
  auto actual = ParseTimestampField(json_object, "timeCreated");
  ASSERT_STATUS_OK(actual);

  using std::chrono::duration_cast;
  EXPECT_EQ(
      0L,
      duration_cast<std::chrono::seconds>(actual->time_since_epoch()).count());
}

TEST(MetadataParserTest, ParseTimestampInvalidType) {
  std::string text = R"""({
      "updated": [0, 1, 2]
})""";
  auto json_object = nlohmann::json::parse(text);
  auto actual = ParseTimestampField(json_object, "updated");
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument));
}

template <typename Integer>
void CheckParseNormal(
    std::function<StatusOr<Integer>(nlohmann::json const&, char const*)>
        tested) {
  std::string text = R"""({ "field": 42 })""";
  auto json_object = nlohmann::json::parse(text);
  auto actual = tested(json_object, "field");
  EXPECT_EQ(Integer(42), actual.value());
}

/// @test Verify Parse*Field can parse regular values.
TEST(MetadataParserTest, ParseIntegralFieldNormal) {
  CheckParseNormal<std::int32_t>(&ParseIntField);
  CheckParseNormal<std::uint32_t>(&ParseUnsignedIntField);
  CheckParseNormal<std::int64_t>(&ParseLongField);
  CheckParseNormal<std::uint64_t>(&ParseUnsignedLongField);
}

template <typename Integer>
void CheckParseFromString(
    std::function<StatusOr<Integer>(nlohmann::json const&, char const*)>
        tested) {
  std::string text = R"""({ "field": "1234" })""";
  auto json_object = nlohmann::json::parse(text);
  auto actual = tested(json_object, "field");
  EXPECT_EQ(Integer(1234), actual.value());
}

/// @test Verify Parse*Field can parse string values.
TEST(MetadataParserTest, ParseIntegralFieldString) {
  CheckParseFromString<std::int32_t>(&ParseIntField);
  CheckParseFromString<std::uint32_t>(&ParseUnsignedIntField);
  CheckParseFromString<std::int64_t>(&ParseLongField);
  CheckParseFromString<std::uint64_t>(&ParseUnsignedLongField);
}

template <typename Integer>
void CheckParseFullRange(
    std::function<StatusOr<Integer>(nlohmann::json const&, char const*)>
        tested) {
  auto min = tested(
      nlohmann::json{
          {"field", std::to_string(std::numeric_limits<Integer>::min())}},
      "field");
  EXPECT_EQ(std::numeric_limits<Integer>::min(), min.value());
  auto max = tested(
      nlohmann::json{
          {"field", std::to_string(std::numeric_limits<Integer>::max())}},
      "field");
  EXPECT_EQ(std::numeric_limits<Integer>::max(), max.value());
}

/// @test Verify Parse*Field can parse string values.
TEST(MetadataParserTest, ParseIntegralFieldFullRange) {
  CheckParseFullRange<std::int32_t>(&ParseIntField);
  CheckParseFullRange<std::uint32_t>(&ParseUnsignedIntField);
  CheckParseFullRange<std::int64_t>(&ParseLongField);
  CheckParseFullRange<std::uint64_t>(&ParseUnsignedLongField);
}

template <typename Integer>
void CheckParseMissing(
    std::function<StatusOr<Integer>(nlohmann::json const&, char const*)>
        tested) {
  std::string text = R"""({ "field": "1234" })""";
  auto json_object = nlohmann::json::parse(text);
  auto actual = tested(json_object, "some-other-field");
  EXPECT_EQ(Integer(0), actual.value());
}

/// @test Verify Parse*Field can parse string values.
TEST(MetadataParserTest, ParseIntegralFieldMissing) {
  CheckParseMissing<std::int32_t>(&ParseIntField);
  CheckParseMissing<std::uint32_t>(&ParseUnsignedIntField);
  CheckParseMissing<std::int64_t>(&ParseLongField);
  CheckParseMissing<std::uint64_t>(&ParseUnsignedLongField);
}

template <typename Integer>
void CheckParseInvalid(
    std::function<StatusOr<Integer>(nlohmann::json const&, char const*)>
        tested) {
  std::string text = R"""({ "field_name": "not-a-number" })""";
  auto json_object = nlohmann::json::parse(text);
  EXPECT_THAT(tested(json_object, "field_name"),
              StatusIs(StatusCode::kInvalidArgument));
}

/// @test Verify Parse*Field detects invalid values.
TEST(MetadataParserTest, ParseIntegralFieldInvalid) {
  CheckParseInvalid<std::int32_t>(&ParseIntField);
  CheckParseInvalid<std::uint32_t>(&ParseUnsignedIntField);
  CheckParseInvalid<std::int64_t>(&ParseLongField);
  CheckParseInvalid<std::uint64_t>(&ParseUnsignedLongField);
}

template <typename Integer>
void CheckParseInvalidFieldType(
    std::function<StatusOr<Integer>(nlohmann::json const&, char const*)>
        tested) {
  std::string text = R"""({ "field_name": [0, 1, 2] })""";
  auto json_object = nlohmann::json::parse(text);
  EXPECT_THAT(tested(json_object, "field_name"),
              StatusIs(StatusCode::kInvalidArgument));
}

/// @test Verify Parse*Field detects invalid field types.
TEST(MetadataParserTest, ParseIntegralFieldInvalidFieldType) {
  CheckParseInvalidFieldType<std::int32_t>(&ParseIntField);
  CheckParseInvalidFieldType<std::uint32_t>(&ParseUnsignedIntField);
  CheckParseInvalidFieldType<std::int64_t>(&ParseLongField);
  CheckParseInvalidFieldType<std::uint64_t>(&ParseUnsignedLongField);
}

TEST(MetadataParserTest, NotJsonObject) {
  EXPECT_THAT(NotJsonObject(nlohmann::json{}, GCP_ERROR_INFO()),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(NotJsonObject(nlohmann::json{"1234"}, GCP_ERROR_INFO()),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(NotJsonObject(nlohmann::json{std::vector<int>{1, 2, 3}},
                            GCP_ERROR_INFO()),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(NotJsonObject(nlohmann::json::parse("", nullptr, false),
                            GCP_ERROR_INFO()),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(NotJsonObject(nlohmann::json::parse("{ invalid", nullptr, false),
                            GCP_ERROR_INFO()),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(
      NotJsonObject(nlohmann::json::parse(R"js("abc")js", nullptr, false),
                    GCP_ERROR_INFO()),
      StatusIs(StatusCode::kInvalidArgument));
}

TEST(MetadataParserTest, ExpectedJsonObject) {
  EXPECT_THAT(ExpectedJsonObject("", GCP_ERROR_INFO()),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(ExpectedJsonObject("123", GCP_ERROR_INFO()),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(ExpectedJsonObject("{", GCP_ERROR_INFO()),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(
      ExpectedJsonObject("01234567890123456789012345678901234567890123456789",
                         GCP_ERROR_INFO()),
      StatusIs(StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
