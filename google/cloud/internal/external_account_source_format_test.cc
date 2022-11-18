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

#include "google/cloud/internal/external_account_source_format.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
using ::testing::IsSupersetOf;
using ::testing::Pair;

internal::ErrorContext MakeTestErrorContext() {
  return internal::ErrorContext{
      {{"filename", "my-credentials.json"}, {"key", "value"}}};
}

TEST(ParseExternalAccountSourceFormat, ValidText) {
  auto const creds = nlohmann::json{
      {"file", "/var/run/token-file.txt"},
      {"format", nlohmann::json{{"type", "text"}}},
  };
  auto const format =
      ParseExternalAccountSourceFormat(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(format);
  EXPECT_EQ(format->type, "text");
}

TEST(ParseExternalAccountSourceFormat, ValidJson) {
  auto const creds = nlohmann::json{
      {"file", "/var/run/token-file.txt"},
      {"format", nlohmann::json{{"type", "json"},
                                {"subject_token_field_name", "fieldName"}}},
  };
  auto const format =
      ParseExternalAccountSourceFormat(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(format);
  EXPECT_EQ(format->type, "json");
  EXPECT_EQ(format->subject_token_field_name, "fieldName");
}

TEST(ParseExternalAccountSourceFormat, MissingIsText) {
  auto const creds = nlohmann::json{
      {"file", "/var/run/token-file.txt"},
  };
  auto const format =
      ParseExternalAccountSourceFormat(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(format);
  EXPECT_EQ(format->type, "text");
}

TEST(ParseExternalAccountSourceFormat, MissingTypeIsText) {
  auto const creds = nlohmann::json{
      {"file", "/var/run/token-file.txt"},
      {"format", nlohmann::json{{"unused", "value"}}},
  };
  auto const format =
      ParseExternalAccountSourceFormat(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(format);
  EXPECT_EQ(format->type, "text");
}

TEST(ParseExternalAccountSourceFormat, InvalidFormatType) {
  auto const creds = nlohmann::json{
      {"file", "/var/run/token-file.txt"},
      {"format", {{"type", true}}},
  };
  auto const format =
      ParseExternalAccountSourceFormat(creds, MakeTestErrorContext());
  EXPECT_THAT(format, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("invalid type for `type` field")));
  EXPECT_THAT(format.status().error_info().metadata(),
              IsSupersetOf({Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

TEST(ParseExternalAccountSourceFormat, UnknownFormatType) {
  auto const creds = nlohmann::json{
      {"file", "/var/run/token-file.txt"},
      {"format", {{"type", "neither-json-nor-text"}}},
  };
  auto const format =
      ParseExternalAccountSourceFormat(creds, MakeTestErrorContext());
  EXPECT_THAT(format,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("invalid file type <neither-json-nor-text>")));
  EXPECT_THAT(format.status().error_info().metadata(),
              IsSupersetOf({Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

TEST(ParseExternalAccountSourceFormat, MissingFormatSubject) {
  auto const creds = nlohmann::json{
      {"file", "/var/run/token-file.json"},
      {"format", {{"type", "json"}}},
  };
  auto const format =
      ParseExternalAccountSourceFormat(creds, MakeTestErrorContext());
  EXPECT_THAT(format, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("`subject_token_field_name`")));
  EXPECT_THAT(format.status().error_info().metadata(),
              IsSupersetOf({Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

TEST(ParseExternalAccountSourceFormat, InvalidFormatSubject) {
  auto const creds = nlohmann::json{
      {"file", "/var/run/token-file.json"},
      {"format", {{"type", "json"}, {"subject_token_field_name", true}}},
  };
  auto const format =
      ParseExternalAccountSourceFormat(creds, MakeTestErrorContext());
  EXPECT_THAT(format, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("`subject_token_field_name`")));
  EXPECT_THAT(format.status().error_info().metadata(),
              IsSupersetOf({Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
