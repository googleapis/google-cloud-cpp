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

#include "google/cloud/storage/internal/metadata_parser.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
/// @test Verify that we parse boolean values in JSON objects.
TEST(MetadataParserTest, ParseBoolField) {
  std::string text = R"""({
      "flag1": true,
      "flag2": false
})""";
  auto json_object = nl::json::parse(text);
  EXPECT_TRUE(ParseBoolField(json_object, "flag1"));
  EXPECT_FALSE(ParseBoolField(json_object, "flag2"));
}

/// @test Verify that we parse boolean values in JSON objects.
TEST(MetadataParserTest, ParseBoolFieldFromString) {
  std::string text = R"""({
      "flag1": "true",
      "flag2": "false"
})""";
  auto json_object = nl::json::parse(text);
  EXPECT_TRUE(ParseBoolField(json_object, "flag1"));
  EXPECT_FALSE(ParseBoolField(json_object, "flag2"));
}

/// @test Verify that we parse missing boolean values in JSON objects.
TEST(MetadataParserTest, ParseMissingBoolField) {
  std::string text = R"""({
      "flag": true
})""";
  auto json_object = nl::json::parse(text);
  auto actual = ParseBoolField(json_object, "some-other-flag");
  EXPECT_FALSE(actual);
}

/// @test Verify that we raise an exception with invalid boolean fields.
TEST(MetadataParserTest, ParseInvalidBoolFieldValue) {
  std::string text = R"""({
      "flag": "not-a-boolean"
})""";
  auto json_object = nl::json::parse(text);

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseBoolField(json_object, "flag"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseBoolField(json_object, "flag"), "");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that we raise an exception with invalid boolean fields.
TEST(MetadataParserTest, ParseInvalidBoolFieldType) {
  std::string text = R"""({
      "flag": [0, 1, 2]
})""";
  auto json_object = nl::json::parse(text);

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseBoolField(json_object, "flag"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseBoolField(json_object, "flag"), "");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that we parse RFC-3339 timestamps in JSON objects.
TEST(MetadataParserTest, ParseTimestampField) {
  std::string text = R"""({
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z"
})""";
  auto json_object = nl::json::parse(text);
  auto actual = ParseTimestampField(json_object, "timeCreated");

  // Use `date -u +%s --date='2018-05-19T19:31:14Z'` to get the magic number:
  using std::chrono::duration_cast;
  EXPECT_EQ(
      1526758274L,
      duration_cast<std::chrono::seconds>(actual.time_since_epoch()).count());
}

/// @test Verify that we parse RFC-3339 timestamps in JSON objects.
TEST(MetadataParserTest, ParseMissingTimestampField) {
  std::string text = R"""({
      "updated": "2018-05-19T19:31:24Z"
})""";
  auto json_object = nl::json::parse(text);
  auto actual = ParseTimestampField(json_object, "timeCreated");

  using std::chrono::duration_cast;
  EXPECT_EQ(
      0L,
      duration_cast<std::chrono::seconds>(actual.time_since_epoch()).count());
}

template <typename Integer>
void CheckParseNormal(
    std::function<Integer(nl::json const&, char const*)> tested) {
  std::string text = R"""({ "field": 42 })""";
  auto json_object = nl::json::parse(text);
  auto actual = tested(json_object, "field");
  EXPECT_EQ(Integer(42), actual);
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
    std::function<Integer(nl::json const&, char const*)> tested) {
  std::string text = R"""({ "field": "1234" })""";
  auto json_object = nl::json::parse(text);
  auto actual = tested(json_object, "field");
  EXPECT_EQ(Integer(1234), actual);
}

/// @test Verify Parse*Field can parse string values.
TEST(MetadataParserTest, ParseIntegralFieldString) {
  CheckParseFromString<std::int32_t>(&ParseIntField);
  CheckParseFromString<std::uint32_t>(&ParseUnsignedIntField);
  CheckParseFromString<std::int64_t>(&ParseLongField);
  CheckParseFromString<std::uint64_t>(&ParseUnsignedLongField);
}

template <typename Integer>
void CheckParseMissing(
    std::function<Integer(nl::json const&, char const*)> tested) {
  std::string text = R"""({ "field": "1234" })""";
  auto json_object = nl::json::parse(text);
  auto actual = tested(json_object, "some-other-field");
  EXPECT_EQ(Integer(0), actual);
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
    std::function<Integer(nl::json const&, char const*)> tested) {
  std::string text = R"""({ "field_name": "not-a-number" })""";
  auto json_object = nl::json::parse(text);
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(tested(json_object, "field_name"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested(json_object, "field_name"), "");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
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
    std::function<Integer(nl::json const&, char const*)> tested) {
  std::string text = R"""({ "field_name": [0, 1, 2] })""";
  auto json_object = nl::json::parse(text);
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try {
        tested(json_object, "field_name");
      } catch (std::exception const& ex) {
        EXPECT_THAT(ex.what(), ::testing::HasSubstr("json="));
        EXPECT_THAT(ex.what(), ::testing::HasSubstr("<field_name>"));
        EXPECT_THAT(ex.what(), ::testing::HasSubstr("[0,1,2]"));
        throw;
      },
      std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested(json_object, "field_name"), "");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify Parse*Field detects invalid field types.
TEST(MetadataParserTest, ParseIntegralFieldInvalidFieldType) {
  CheckParseInvalidFieldType<std::int32_t>(&ParseIntField);
  CheckParseInvalidFieldType<std::uint32_t>(&ParseUnsignedIntField);
  CheckParseInvalidFieldType<std::int64_t>(&ParseLongField);
  CheckParseInvalidFieldType<std::uint64_t>(&ParseUnsignedLongField);
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
