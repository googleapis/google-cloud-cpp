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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_EXTERNAL_ACCOUNT_PARSING_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_EXTERNAL_ACCOUNT_PARSING_H

#include "google/cloud/internal/error_metadata.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/strings/string_view.h"
#include <nlohmann/json.hpp>
#include <string>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Returns the string value for `json[name]` (which must exist) or a
/// descriptive error.
StatusOr<std::string> ValidateStringField(nlohmann::json const& json,
                                          absl::string_view name,
                                          absl::string_view object_name,
                                          internal::ErrorContext const& ec);

/// Returns the string value for `json[name]`, a default value if it does not
/// exist, or a descriptive error if it exists but it is not a string.
StatusOr<std::string> ValidateStringField(nlohmann::json const& json,
                                          absl::string_view name,
                                          absl::string_view object_name,
                                          absl::string_view default_value,
                                          internal::ErrorContext const& ec);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_EXTERNAL_ACCOUNT_PARSING_H
