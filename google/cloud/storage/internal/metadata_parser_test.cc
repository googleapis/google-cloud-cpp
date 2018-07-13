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

using namespace google::cloud::storage::internal;

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

/// @test Verify that we parse long values in JSON objects.
TEST(MetadataParserTest, ParseLongField) {
  std::string text = R"""({
      "counter": 42
})""";
  auto json_object = nl::json::parse(text);
  auto actual = ParseLongField(json_object, "counter");

  EXPECT_EQ(42L, actual);
}

/// @test Verify that we parse long values in JSON objects.
TEST(MetadataParserTest, ParseLongFieldFromString) {
  std::string text = R"""({
      "counter": "42"
})""";
  auto json_object = nl::json::parse(text);
  auto actual = ParseLongField(json_object, "counter");

  EXPECT_EQ(42L, actual);
}

/// @test Verify that we parse missing long values in JSON objects.
TEST(MetadataParserTest, ParseMissingLongField) {
  std::string text = R"""({
      "counter": "42"
})""";
  auto json_object = nl::json::parse(text);
  auto actual = ParseLongField(json_object, "some-other-counter");

  EXPECT_EQ(0L, actual);
}

/// @test Verify that we raise an exception with invalid long fields.
TEST(MetadataParserTest, ParseInvalidLongFieldValue) {
  std::string text = R"""({
      "counter": "not-a-number"
})""";
  auto json_object = nl::json::parse(text);

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseLongField(json_object, "counter"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseLongField(json_object, "counter"), "");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that we raise an exception with invalid long fields.
TEST(MetadataParserTest, ParseInvalidLongFieldType) {
  std::string text = R"""({
      "counter": [0, 1, 2]
})""";
  auto json_object = nl::json::parse(text);

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseLongField(json_object, "counter"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseLongField(json_object, "counter"), "");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that we parse long values in JSON objects.
TEST(MetadataParserTest, ParseUnsignedLongField) {
  std::string text = R"""({
      "size": 42
})""";
  auto json_object = nl::json::parse(text);
  auto actual = ParseUnsignedLongField(json_object, "size");

  EXPECT_EQ(42UL, actual);
}

/// @test Verify that we parse long values in JSON objects.
TEST(MetadataParserTest, ParseUnsignedLongFieldFromString) {
  std::string text = R"""({
      "size": "42"
})""";
  auto json_object = nl::json::parse(text);
  auto actual = ParseUnsignedLongField(json_object, "size");

  EXPECT_EQ(42UL, actual);
}

/// @test Verify that we parse missing long values in JSON objects.
TEST(MetadataParserTest, ParseMissingUnsignedLongField) {
  std::string text = R"""({
      "size": "42"
})""";
  auto json_object = nl::json::parse(text);
  auto actual = ParseUnsignedLongField(json_object, "some-other-size");

  EXPECT_EQ(0UL, actual);
}

/// @test Verify that we raise an exception with invalid long fields.
TEST(MetadataParserTest, ParseInvalidUnsignedLongFieldValue) {
  std::string text = R"""({
      "size": "not-a-number"
})""";
  auto json_object = nl::json::parse(text);

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseUnsignedLongField(json_object, "size"),
               std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseUnsignedLongField(json_object, "size"), "");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that we raise an exception with invalid long fields.
TEST(MetadataParserTest, ParseInvalidUnsignedLongFieldType) {
  std::string text = R"""({
      "size": [0, 1, 2]
})""";
  auto json_object = nl::json::parse(text);

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseUnsignedLongField(json_object, "size"),
               std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseUnsignedLongField(json_object, "size"), "");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}
