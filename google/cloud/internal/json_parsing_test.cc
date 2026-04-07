// Copyright 2022 Google LLC
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

#include "google/cloud/internal/json_parsing.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Pair;

TEST(JsonParsingTest, ValidateStringFieldSuccess) {
  auto const json = nlohmann::json{{"someField", "value"}};
  auto actual = ValidateStringField(
      json, "someField", "test-object",
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, "value");
}

TEST(JsonParsingTest, ValidateStringFieldMissing) {
  auto const json = nlohmann::json{{"some-field", "value"}};
  auto actual = ValidateStringField(
      json, "missingField", "test-object",
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               AllOf(HasSubstr("missingField"),
                                     HasSubstr("test-object"))));
  EXPECT_THAT(actual.status().error_info().metadata(),
              Contains(Pair("filename", "/dev/null")));
  EXPECT_THAT(actual.status().error_info().metadata(),
              Contains(Pair("origin", "test")));
}

TEST(JsonParsingTest, ValidateStringFieldNotString) {
  auto const json =
      nlohmann::json{{"some-field", "value"}, {"wrongType", true}};
  auto actual = ValidateStringField(
      json, "wrongType", "test-object",
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               AllOf(HasSubstr("wrongType"),
                                     HasSubstr("test-object"))));
  EXPECT_THAT(actual.status().error_info().metadata(),
              Contains(Pair("filename", "/dev/null")));
  EXPECT_THAT(actual.status().error_info().metadata(),
              Contains(Pair("origin", "test")));
}

TEST(JsonParsingTest, ValidateStringFieldDefaultSuccess) {
  auto const json = nlohmann::json{{"someField", "value"}};
  auto actual = ValidateStringField(
      json, "someField", "test-object", "default-value",
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, "value");
}

TEST(JsonParsingTest, ValidateStringFieldDefaultMissing) {
  auto const json = nlohmann::json{{"anotherField", "value"}};
  auto actual = ValidateStringField(
      json, "someField", "test-object", "default-value",
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, "default-value");
}

TEST(JsonParsingTest, ValidateStringFieldDefaultNotInt) {
  auto const json =
      nlohmann::json{{"some-field", "value"}, {"wrongType", true}};
  auto actual = ValidateStringField(
      json, "wrongType", "test-object", "default-value",
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               AllOf(HasSubstr("wrongType"),
                                     HasSubstr("test-object"))));
  EXPECT_THAT(actual.status().error_info().metadata(),
              Contains(Pair("filename", "/dev/null")));
  EXPECT_THAT(actual.status().error_info().metadata(),
              Contains(Pair("origin", "test")));
}

TEST(JsonParsingTest, ValidateIntFieldSuccess) {
  auto const json = nlohmann::json{{"someField", 42}};
  auto actual = ValidateIntField(
      json, "someField", "test-object",
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, 42);
}

TEST(JsonParsingTest, ValidateIntFieldMissing) {
  auto const json = nlohmann::json{{"some-field", 42}};
  auto actual = ValidateIntField(
      json, "missingField", "test-object",
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               AllOf(HasSubstr("missingField"),
                                     HasSubstr("test-object"))));
  EXPECT_THAT(actual.status().error_info().metadata(),
              Contains(Pair("filename", "/dev/null")));
  EXPECT_THAT(actual.status().error_info().metadata(),
              Contains(Pair("origin", "test")));
}

TEST(JsonParsingTest, ValidateIntFieldNotString) {
  auto const json =
      nlohmann::json{{"some-field", "value"}, {"wrongType", true}};
  auto actual = ValidateIntField(
      json, "wrongType", "test-object",
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               AllOf(HasSubstr("wrongType"),
                                     HasSubstr("test-object"))));
  EXPECT_THAT(actual.status().error_info().metadata(),
              Contains(Pair("filename", "/dev/null")));
  EXPECT_THAT(actual.status().error_info().metadata(),
              Contains(Pair("origin", "test")));
}

TEST(JsonParsingTest, ValidateIntFieldDefaultSuccess) {
  auto const json = nlohmann::json{{"someField", 42}};
  auto actual = ValidateIntField(
      json, "someField", "test-object", 42,
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, 42);
}

TEST(JsonParsingTest, ValidateIntFieldDefaultMissing) {
  auto const json = nlohmann::json{{"anotherField", "value"}};
  auto actual = ValidateIntField(
      json, "someField", "test-object", 42,
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, 42);
}

TEST(JsonParsingTest, ValidateIntFieldDefaultNotString) {
  auto const json =
      nlohmann::json{{"some-field", "value"}, {"wrongType", true}};
  auto actual = ValidateIntField(
      json, "wrongType", "test-object", 42,
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               AllOf(HasSubstr("wrongType"),
                                     HasSubstr("test-object"))));
  EXPECT_THAT(actual.status().error_info().metadata(),
              Contains(Pair("filename", "/dev/null")));
  EXPECT_THAT(actual.status().error_info().metadata(),
              Contains(Pair("origin", "test")));
}

TEST(JsonParsingTest, ValidateStringArrayFieldSuccess) {
  auto const json = nlohmann::json{{"someField", {"value1", "value2"}}};
  auto actual = ValidateStringArrayField(
      json, "someField", "test-object",
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(*actual, ElementsAre("value1", "value2"));
}

TEST(JsonParsingTest, ValidateStringArrayFieldMissing) {
  auto const json = nlohmann::json{{"some-field", {"value1", "value2"}}};
  auto actual = ValidateStringArrayField(
      json, "missingField", "test-object",
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               AllOf(HasSubstr("missingField"),
                                     HasSubstr("test-object"))));
  EXPECT_THAT(actual.status().error_info().metadata(),
              Contains(Pair("filename", "/dev/null")));
  EXPECT_THAT(actual.status().error_info().metadata(),
              Contains(Pair("origin", "test")));
}

TEST(JsonParsingTest, ValidateStringArrayFieldNotString) {
  auto const json = nlohmann::json({"wrongType", {"value1", true}});
  auto actual = ValidateStringArrayField(
      json, "wrongType", "test-object",
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               AllOf(HasSubstr("wrongType"),
                                     HasSubstr("test-object"))));
  EXPECT_THAT(actual.status().error_info().metadata(),
              Contains(Pair("filename", "/dev/null")));
  EXPECT_THAT(actual.status().error_info().metadata(),
              Contains(Pair("origin", "test")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
