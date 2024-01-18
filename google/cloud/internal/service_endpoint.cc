// Copyright 2023 Google LLC
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

#include "google/cloud/internal/service_endpoint.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/universe_domain_options.h"
#include "absl/strings/strip.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

// TODO(#13122): This function may need to be enhanced to support an env var
// for UniverseDomain.
StatusOr<std::string> DetermineServiceEndpoint(
    absl::optional<std::string> endpoint_env_var,
    absl::optional<std::string> endpoint_option, std::string default_endpoint,
    Options const& options) {
  if (endpoint_env_var.has_value() && !endpoint_env_var->empty()) {
    return *endpoint_env_var;
  }
  if (endpoint_option.has_value()) {
    return *endpoint_option;
  }
  if (!absl::EndsWith(default_endpoint, ".")) {
    absl::StrAppend(&default_endpoint, ".");
  }
  if (options.has<UniverseDomainOption>()) {
    std::string universe_domain_option = options.get<UniverseDomainOption>();
    if (universe_domain_option.empty()) {
      return internal::InvalidArgumentError(
          "UniverseDomainOption cannot be empty");
    }
    if (!absl::StartsWith(universe_domain_option, ".")) {
      universe_domain_option = absl::StrCat(".", universe_domain_option);
    }
    return absl::StrCat(absl::StripSuffix(default_endpoint, ".googleapis.com."),
                        universe_domain_option);
  }
  return default_endpoint;
}

std::string UniverseDomainEndpoint(std::string gdu_endpoint,
                                   Options const& options) {
  if (!options.has<UniverseDomainOption>()) return gdu_endpoint;
  // Support both "foo.googleapis.com" and "foo.googleapis.com."
  return absl::StrCat(absl::StripSuffix(absl::StripSuffix(gdu_endpoint, "."),
                                        ".googleapis.com"),
                      ".", options.get<UniverseDomainOption>());
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
