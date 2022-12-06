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
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/external_account_parsing.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<ExternalAccountSourceFormat> ParseExternalAccountSourceFormat(
    nlohmann::json const& credentials_source,
    internal::ErrorContext const& ec) {
  auto it = credentials_source.find("format");
  if (it == credentials_source.end())
    return ExternalAccountSourceFormat{"text", {}};
  if (!it->is_object()) {
    return InvalidArgumentError(
        "invalid type for `format` field in `credentials_source`",
        GCP_ERROR_INFO().WithContext(ec));
  }
  auto const& format = *it;
  auto type = ValidateStringField(format, "type", "credentials_source.format",
                                  "text", ec);
  if (!type) return std::move(type).status();
  if (*type == "text") return ExternalAccountSourceFormat{"text", {}};
  if (*type != "json") {
    return InvalidArgumentError(
        absl::StrCat("invalid file type <", *type, "> in `credentials_source`"),
        GCP_ERROR_INFO().WithContext(ec));
  }
  auto field = ValidateStringField(format, "subject_token_field_name",
                                   "credentials_source.format", ec);
  if (!field) return std::move(field).status();
  return ExternalAccountSourceFormat{*std::move(type), *std::move(field)};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
