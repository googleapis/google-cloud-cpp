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

#include "google/cloud/internal/external_account_token_source_file.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <cstdio>
#include <fstream>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::IsSupersetOf;
using ::testing::Pair;

using MockClientFactory =
    ::testing::MockFunction<std::unique_ptr<rest_internal::RestClient>(
        Options const&)>;

internal::ErrorContext MakeTestErrorContext() {
  return internal::ErrorContext{
      {{"filename", "my-credentials.json"}, {"key", "value"}}};
}

std::string MakeRandomFilename() {
  static auto generator = internal::DefaultPRNG(std::random_device{}());
  return internal::PathAppend(
      ::testing::TempDir(),
      internal::Sample(generator, 32, "abcdefghijklmnopqrstuvwxyz0123456789"));
}

TEST(ExternalAccountTokenSource, WorkingTextFile) {
  auto const token_filename = MakeRandomFilename();
  auto const token = std::string{"a-test-only-token"};
  std::ofstream(token_filename) << token;
  auto const creds = nlohmann::json{{"file", token_filename}};
  auto const source =
      MakeExternalAccountTokenSourceFile(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);
  MockClientFactory cf;
  EXPECT_CALL(cf, Call).Times(0);
  auto const actual = (*source)(cf.AsStdFunction(), Options{});
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, internal::SubjectToken{token});
  EXPECT_EQ(std::remove(token_filename.c_str()), 0);
}

TEST(ExternalAccountTokenSource, WorkingJsonFile) {
  auto const token_filename = MakeRandomFilename();
  auto const token = std::string{"a-test-only-token"};
  std::ofstream(token_filename)
      << nlohmann::json{{"tokenField", token}, {"unusedField", true}}.dump();
  auto const creds = nlohmann::json{
      {"file", token_filename},
      {"format",
       {{"type", "json"}, {"subject_token_field_name", "tokenField"}}}};
  auto const source =
      MakeExternalAccountTokenSourceFile(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);
  MockClientFactory cf;
  EXPECT_CALL(cf, Call).Times(0);
  auto const actual = (*source)(cf.AsStdFunction(), Options{});
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, internal::SubjectToken{token});
  EXPECT_EQ(std::remove(token_filename.c_str()), 0);
}

TEST(ExternalAccountTokenSource, MissingFileField) {
  auto const creds = nlohmann::json{
      {"file-but-wrong", "/var/tmp/token-file.json"},
      {"format", {{"type", "text"}}},
  };
  auto const source =
      MakeExternalAccountTokenSourceFile(creds, MakeTestErrorContext());
  EXPECT_THAT(source, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("cannot find `file` field")));
  EXPECT_THAT(source.status().error_info().metadata(),
              IsSupersetOf({Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, InvalidFileField) {
  auto const creds = nlohmann::json{
      {"file", true},
      {"format", {{"type", "text"}}},
  };
  auto const source =
      MakeExternalAccountTokenSourceFile(creds, MakeTestErrorContext());
  EXPECT_THAT(source, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("invalid type for `file` field")));
  EXPECT_THAT(source.status().error_info().metadata(),
              IsSupersetOf({Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, UnknownFormatType) {
  auto const creds = nlohmann::json{
      {"file", "/var/run/token-file.txt"},
      {"format", {{"type", "neither-json-nor-text"}}},
  };
  auto const format =
      MakeExternalAccountTokenSourceFile(creds, MakeTestErrorContext());
  EXPECT_THAT(format,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("invalid file type <neither-json-nor-text>")));
  EXPECT_THAT(
      format.status().error_info().metadata(),
      IsSupersetOf(
          {Pair("credentials_source.type", "file"),
           Pair("credentials_source.file.filename", "/var/run/token-file.txt"),
           Pair("filename", "my-credentials.json"), Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, MissingTextFile) {
  auto const token_filename = MakeRandomFilename();
  auto const creds = nlohmann::json{{"file", token_filename}};
  auto const source =
      MakeExternalAccountTokenSourceFile(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);
  MockClientFactory cf;
  EXPECT_CALL(cf, Call).Times(0);
  auto const actual = (*source)(cf.AsStdFunction(), Options{});
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("error reading subject token file")));
  EXPECT_THAT(
      actual.status().error_info().metadata(),
      IsSupersetOf(
          {Pair("credentials_source.type", "file"),
           Pair("credentials_source.file.type", "text"),
           Pair("credentials_source.file.filename", token_filename.c_str()),
           Pair("filename", "my-credentials.json"), Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, MissingJsonFile) {
  auto const token_filename = MakeRandomFilename();
  auto const token = std::string{"a-test-only-token"};
  auto const creds = nlohmann::json{
      {"file", token_filename},
      {"format",
       {{"type", "json"}, {"subject_token_field_name", "tokenField"}}}};
  auto const source =
      MakeExternalAccountTokenSourceFile(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);
  MockClientFactory cf;
  EXPECT_CALL(cf, Call).Times(0);
  auto const actual = (*source)(cf.AsStdFunction(), Options{});
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("error reading subject token file")));
  EXPECT_THAT(
      actual.status().error_info().metadata(),
      IsSupersetOf(
          {Pair("credentials_source.type", "file"),
           Pair("credentials_source.file.type", "json"),
           Pair("credentials_source.file.filename", token_filename.c_str()),
           Pair("credentials_source.file.source_token_field_name",
                "tokenField"),
           Pair("filename", "my-credentials.json"), Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, JsonFileIsNotJson) {
  auto const token_filename = MakeRandomFilename();
  std::ofstream(token_filename) << "not-a-json-object";
  auto const token = std::string{"a-test-only-token"};
  auto const creds = nlohmann::json{
      {"file", token_filename},
      {"format",
       {{"type", "json"}, {"subject_token_field_name", "tokenField"}}}};
  auto const source =
      MakeExternalAccountTokenSourceFile(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);
  MockClientFactory cf;
  EXPECT_CALL(cf, Call).Times(0);
  auto const actual = (*source)(cf.AsStdFunction(), Options{});
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               AllOf(HasSubstr("parse error in JSON object"),
                                     HasSubstr(token_filename))));
  EXPECT_THAT(
      actual.status().error_info().metadata(),
      IsSupersetOf(
          {Pair("credentials_source.type", "file"),
           Pair("credentials_source.file.type", "json"),
           Pair("credentials_source.file.filename", token_filename.c_str()),
           Pair("credentials_source.file.source_token_field_name",
                "tokenField"),
           Pair("filename", "my-credentials.json"), Pair("key", "value")}));
  EXPECT_EQ(std::remove(token_filename.c_str()), 0);
}

TEST(ExternalAccountTokenSource, JsonFileIsNotJsonObject) {
  auto const token_filename = MakeRandomFilename();
  std::ofstream(token_filename) << nlohmann::json("not-json-object").dump();
  auto const token = std::string{"a-test-only-token"};
  auto const creds = nlohmann::json{
      {"file", token_filename},
      {"format",
       {{"type", "json"}, {"subject_token_field_name", "tokenField"}}}};
  auto const source =
      MakeExternalAccountTokenSourceFile(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);
  MockClientFactory cf;
  EXPECT_CALL(cf, Call).Times(0);
  auto const actual = (*source)(cf.AsStdFunction(), Options{});
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               AllOf(HasSubstr("parse error in JSON object"),
                                     HasSubstr(token_filename))));
  EXPECT_THAT(
      actual.status().error_info().metadata(),
      IsSupersetOf(
          {Pair("credentials_source.type", "file"),
           Pair("credentials_source.file.type", "json"),
           Pair("credentials_source.file.filename", token_filename.c_str()),
           Pair("credentials_source.file.source_token_field_name",
                "tokenField"),
           Pair("filename", "my-credentials.json"), Pair("key", "value")}));
  EXPECT_EQ(std::remove(token_filename.c_str()), 0);
}

TEST(ExternalAccountTokenSource, JsonFileMissingField) {
  auto const token_filename = MakeRandomFilename();
  std::ofstream(token_filename) << nlohmann::json{{"wrongField", "value"}};
  auto const token = std::string{"a-test-only-token"};
  auto const creds = nlohmann::json{
      {"file", token_filename},
      {"format",
       {{"type", "json"}, {"subject_token_field_name", "tokenField"}}}};
  auto const source =
      MakeExternalAccountTokenSourceFile(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);
  MockClientFactory cf;
  EXPECT_CALL(cf, Call).Times(0);
  auto const actual = (*source)(cf.AsStdFunction(), Options{});
  EXPECT_THAT(
      actual,
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("subject token field not found in JSON object"),
                     HasSubstr(token_filename))));
  EXPECT_THAT(
      actual.status().error_info().metadata(),
      IsSupersetOf(
          {Pair("credentials_source.type", "file"),
           Pair("credentials_source.file.type", "json"),
           Pair("credentials_source.file.filename", token_filename.c_str()),
           Pair("credentials_source.file.source_token_field_name",
                "tokenField"),
           Pair("filename", "my-credentials.json"), Pair("key", "value")}));
  EXPECT_EQ(std::remove(token_filename.c_str()), 0);
}

TEST(ExternalAccountTokenSource, JsonFileInvalidField) {
  auto const token_filename = MakeRandomFilename();
  std::ofstream(token_filename) << nlohmann::json{{"tokenField", true}};
  auto const token = std::string{"a-test-only-token"};
  auto const creds = nlohmann::json{
      {"file", token_filename},
      {"format",
       {{"type", "json"}, {"subject_token_field_name", "tokenField"}}}};
  auto const source =
      MakeExternalAccountTokenSourceFile(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);
  MockClientFactory cf;
  EXPECT_CALL(cf, Call).Times(0);
  auto const actual = (*source)(cf.AsStdFunction(), Options{});
  EXPECT_THAT(
      actual,
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("invalid type for token field in JSON object"),
                     HasSubstr(token_filename))));
  EXPECT_THAT(
      actual.status().error_info().metadata(),
      IsSupersetOf(
          {Pair("credentials_source.type", "file"),
           Pair("credentials_source.file.type", "json"),
           Pair("credentials_source.file.filename", token_filename.c_str()),
           Pair("credentials_source.file.source_token_field_name",
                "tokenField"),
           Pair("filename", "my-credentials.json"), Pair("key", "value")}));
  EXPECT_EQ(std::remove(token_filename.c_str()), 0);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
