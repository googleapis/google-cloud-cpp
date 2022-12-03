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

#include "google/cloud/internal/external_account_parsing.h"
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
using ::testing::HasSubstr;
using ::testing::Pair;

TEST(ExternalAccountParsing, ValidateStringFieldSuccess) {
  auto const json = nlohmann::json{{"someField", "value"}};
  auto actual = ValidateStringField(
      json, "someField", "test-object",
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, "value");
}

TEST(ExternalAccountParsing, ValidateStringFieldMissing) {
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

TEST(ExternalAccountParsing, ValidateStringFieldNotString) {
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

TEST(ExternalAccountParsing, ValidateStringFieldDefaultSuccess) {
  auto const json = nlohmann::json{{"someField", "value"}};
  auto actual = ValidateStringField(
      json, "someField", "test-object", "default-value",
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, "value");
}

TEST(ExternalAccountParsing, ValidateStringFieldDefaultMissing) {
  auto const json = nlohmann::json{{"anotherField", "value"}};
  auto actual = ValidateStringField(
      json, "someField", "test-object", "default-value",
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, "default-value");
}

TEST(ExternalAccountParsing, ValidateStringFieldDefaultNotString) {
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

TEST(ExternalAccountParsing, ValidateIntFieldSuccess) {
  auto const json = nlohmann::json{{"someField", 42}};
  auto actual = ValidateIntField(
      json, "someField", "test-object",
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, 42);
}

TEST(ExternalAccountParsing, ValidateIntFieldMissing) {
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

TEST(ExternalAccountParsing, ValidateIntFieldNotString) {
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

TEST(ExternalAccountParsing, ValidateIntFieldDefaultSuccess) {
  auto const json = nlohmann::json{{"someField", 42}};
  auto actual = ValidateIntField(
      json, "someField", "test-object", 42,
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, 42);
}

TEST(ExternalAccountParsing, ValidateIntFieldDefaultMissing) {
  auto const json = nlohmann::json{{"anotherField", "value"}};
  auto actual = ValidateIntField(
      json, "someField", "test-object", 42,
      internal::ErrorContext({{"origin", "test"}, {"filename", "/dev/null"}}));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, 42);
}

TEST(ExternalAccountParsing, ValidateIntFieldDefaultNotString) {
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

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
