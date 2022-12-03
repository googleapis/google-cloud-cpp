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
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<std::string> ValidateStringField(nlohmann::json const& json,
                                          absl::string_view name,
                                          absl::string_view object_name,
                                          internal::ErrorContext const& ec) {
  auto it = json.find(std::string{name});
  if (it == json.end()) return MissingFieldError(name, object_name, ec);
  if (!it->is_string()) return InvalidTypeError(name, object_name, ec);
  return it->get<std::string>();
}

StatusOr<std::string> ValidateStringField(nlohmann::json const& json,
                                          absl::string_view name,
                                          absl::string_view object_name,
                                          absl::string_view default_value,
                                          internal::ErrorContext const& ec) {
  auto it = json.find(std::string{name});
  if (it == json.end()) return std::string{default_value};
  if (!it->is_string()) return InvalidTypeError(name, object_name, ec);
  return it->get<std::string>();
}

StatusOr<std::int32_t> ValidateIntField(nlohmann::json const& json,
                                        absl::string_view name,
                                        absl::string_view object_name,
                                        internal::ErrorContext const& ec) {
  auto it = json.find(std::string{name});
  if (it == json.end()) return MissingFieldError(name, object_name, ec);
  if (!it->is_number_integer()) return InvalidTypeError(name, object_name, ec);
  return it->get<std::int32_t>();
}

StatusOr<std::int32_t> ValidateIntField(nlohmann::json const& json,
                                        absl::string_view name,
                                        absl::string_view object_name,
                                        std::int32_t default_value,
                                        internal::ErrorContext const& ec) {
  auto it = json.find(std::string{name});
  if (it == json.end()) return default_value;
  if (!it->is_number_integer()) return InvalidTypeError(name, object_name, ec);
  return it->get<std::int32_t>();
}

Status MissingFieldError(absl::string_view name, absl::string_view object_name,
                         internal::ErrorContext const& ec) {
  return InvalidArgumentError(
      absl::StrCat("cannot find `", name, "` field in `", object_name, "`"),
      GCP_ERROR_INFO().WithContext(ec));
}

Status InvalidTypeError(absl::string_view name, absl::string_view object_name,
                        internal::ErrorContext const& ec) {
  return InvalidArgumentError(absl::StrCat("invalid type for `", name,
                                           "` field in `", object_name, "`"),
                              GCP_ERROR_INFO().WithContext(ec));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
