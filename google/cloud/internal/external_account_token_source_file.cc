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
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/external_account_parsing.h"
#include "google/cloud/internal/make_status.h"
#include <fstream>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

Status BadFile(internal::ErrorContext const& ec) {
  return InvalidArgumentError("error reading subject token file",
                              GCP_ERROR_INFO().WithContext(ec));
}

StatusOr<internal::SubjectToken> TextFileReader(
    std::string const& filename, internal::ErrorContext const& ec) {
  std::ifstream is(filename);
  auto contents = std::string{std::istreambuf_iterator<char>(is.rdbuf()), {}};
  if (!is.is_open() || is.bad()) return BadFile(ec);
  return internal::SubjectToken{std::move(contents)};
}

StatusOr<internal::SubjectToken> JsonFileReader(
    std::string const& filename, std::string const& field_name,
    internal::ErrorContext const& ec) {
  std::ifstream is(filename);
  auto contents = std::string{std::istreambuf_iterator<char>(is.rdbuf()), {}};
  if (!is.is_open() || is.bad()) return BadFile(ec);
  auto json = nlohmann::json::parse(contents, nullptr, false);
  auto error_details = [&](std::string const& msg) {
    return msg + " in JSON object loaded from `" + filename +
           "`, with subject_token_field `" + field_name + "`";
  };
  if (!json.is_object()) {
    return InvalidArgumentError(error_details("parse error"),
                                GCP_ERROR_INFO().WithContext(ec));
  }
  auto it = json.find(field_name);
  if (it == json.end()) {
    return InvalidArgumentError(error_details("subject token field not found"),
                                GCP_ERROR_INFO().WithContext(ec));
  }
  if (!it->is_string()) {
    return InvalidArgumentError(error_details("invalid type for token field"),
                                GCP_ERROR_INFO().WithContext(ec));
  }
  return internal::SubjectToken{it->get<std::string>()};
}

struct Format {
  std::string type;
  std::string subject_token_field_name;
};

StatusOr<Format> ParseFormat(nlohmann::json const& credentials_source,
                             internal::ErrorContext const& ec) {
  auto it = credentials_source.find("format");
  if (it == credentials_source.end()) return Format{"text", {}};
  if (!it->is_object()) {
    return InvalidArgumentError(
        "invalid type for `format` field in `credentials_source`",
        GCP_ERROR_INFO().WithContext(ec));
  }
  auto const& format = *it;
  auto type = ValidateStringField(format, "type", "credentials_source.format",
                                  "text", ec);
  if (!type) return std::move(type).status();
  if (*type == "text") return Format{"text", {}};
  if (*type != "json") {
    return InvalidArgumentError(
        absl::StrCat("invalid file type <", *type, "> in `credentials_source`"),
        GCP_ERROR_INFO().WithContext(ec));
  }
  auto field = ValidateStringField(format, "subject_token_field_name",
                                   "credentials_source.format", ec);
  if (!field) return std::move(field).status();
  return Format{*std::move(type), *std::move(field)};
}

}  // namespace

StatusOr<ExternalAccountTokenSource> MakeExternalAccountTokenSourceFile(
    nlohmann::json const& credentials_source,
    internal::ErrorContext const& ec) {
  auto file =
      ValidateStringField(credentials_source, "file", "credentials_source", ec);
  if (!file) return std::move(file).status();

  // Make a copy, most of the time this function should succeed, and we need the
  // copy for the lambda captures.
  auto context = ec;
  context.emplace_back("credentials_source.type", "file");
  context.emplace_back("credentials_source.file.filename", *file);
  auto format = ParseFormat(credentials_source, context);
  if (!format) return std::move(format).status();
  if (format->type == "text") {
    context.emplace_back("credentials_source.file.type", "text");
    return ExternalAccountTokenSource{
        [f = *std::move(file), ec = std::move(context)](Options const&) {
          return TextFileReader(f, ec);
        }};
  }
  context.emplace_back("credentials_source.file.type", "json");
  context.emplace_back("credentials_source.file.source_token_field_name",
                       format->subject_token_field_name);
  return ExternalAccountTokenSource{[f = *std::move(file),
                                     field = format->subject_token_field_name,
                                     ec = std::move(context)](Options const&) {
    return JsonFileReader(f, field, ec);
  }};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
