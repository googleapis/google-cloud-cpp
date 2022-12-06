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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_EXTERNAL_ACCOUNT_SOURCE_FORMAT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_EXTERNAL_ACCOUNT_SOURCE_FORMAT_H

#include "google/cloud/internal/error_context.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <nlohmann/json.hpp>
#include <string>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The format for external account subject token sources.
 *
 * External accounts credentials use [OAuth 2.0 Token Exchange][RFC 8693] to
 * convert a "subject token" into an "access token". The latter is used (as one
 * would expect) to access GCP services.
 *
 * Some of these sources can return the subject tokens as plain text data, or as
 * a string field in a JSON object.  `ParseExternalAccountSourceFormat()`
 * validates the external source configuration, and returns this struct when
 * the validation is successful.
 *
 * [RFC 8693]: https://www.rfc-editor.org/rfc/rfc8693.html
 */
struct ExternalAccountSourceFormat {
  std::string type;
  std::string subject_token_field_name;
};

StatusOr<ExternalAccountSourceFormat> ParseExternalAccountSourceFormat(
    nlohmann::json const& credentials_source, internal::ErrorContext const& ec);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_EXTERNAL_ACCOUNT_SOURCE_FORMAT_H
