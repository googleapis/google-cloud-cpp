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

/// @test Verify that we parse RFC-3339 timestamps in JSON objects.
TEST(MetadataParserTest, ParseTimestampField) {
  std::string text = R"""({
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z"
})""";
  auto json = storage::internal::nl::json::parse(text);
  auto actual = storage::internal::MetadataParser::ParseTimestampField(
      json, "timeCreated");

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
  auto json = storage::internal::nl::json::parse(text);
  auto actual = storage::internal::MetadataParser::ParseTimestampField(
      json, "timeCreated");

  using std::chrono::duration_cast;
  EXPECT_EQ(
      0L,
      duration_cast<std::chrono::seconds>(actual.time_since_epoch()).count());
}

/// @test Verify that we parse long values in JSON objects.
TEST(MetadataParserTest, ParseLongField) {
  std::string text = R"""({
      "counter": 42
})""";
  auto json = storage::internal::nl::json::parse(text);
  auto actual =
      storage::internal::MetadataParser::ParseLongField(json, "counter");

  EXPECT_EQ(42L, actual);
}

/// @test Verify that we parse long values in JSON objects.
TEST(MetadataParserTest, ParseLongFieldFromString) {
  std::string text = R"""({
      "counter": "42"
})""";
  auto json = storage::internal::nl::json::parse(text);
  auto actual =
      storage::internal::MetadataParser::ParseLongField(json, "counter");

  EXPECT_EQ(42L, actual);
}

/// @test Verify that we parse missing long values in JSON objects.
TEST(MetadataParserTest, ParseMissinLongField) {
  std::string text = R"""({
      "counter": "42"
})""";
  auto json = storage::internal::nl::json::parse(text);
  auto actual = storage::internal::MetadataParser::ParseLongField(
      json, "some-other-counter");

  EXPECT_EQ(0L, actual);
}

/// @test Verify that we raise an exception with invalid long fields.
TEST(MetadataParserTest, ParseInvalidLongFieldValue) {
  std::string text = R"""({
      "counter": "not-a-number"
})""";
  auto json = storage::internal::nl::json::parse(text);

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      storage::internal::MetadataParser::ParseLongField(json, "counter"),
      std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      storage::internal::MetadataParser::ParseLongField(json, "counter"), "");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that we raise an exception with invalid long fields.
TEST(MetadataParserTest, ParseInvalidLongFieldType) {
  std::string text = R"""({
      "counter": [0, 1, 2]
})""";
  auto json = storage::internal::nl::json::parse(text);

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      storage::internal::MetadataParser::ParseLongField(json, "counter"),
      std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      storage::internal::MetadataParser::ParseLongField(json, "counter"), "");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}
