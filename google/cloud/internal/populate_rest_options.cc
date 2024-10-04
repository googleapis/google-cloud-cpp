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

#include "google/cloud/internal/populate_rest_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/credentials_impl.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/populate_common_options.h"
#include "google/cloud/internal/rest_options.h"
#include "google/cloud/rest_options.h"
#include "absl/strings/match.h"
#include "absl/strings/strip.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

Options PopulateRestOptions(Options opts) {
  if (opts.has<ApiKeyOption>() && opts.has<UnifiedCredentialsOption>()) {
    opts.set<UnifiedCredentialsOption>(
        internal::MakeErrorCredentials(internal::InvalidArgumentError(
            "API Keys and Credentials are mutually exclusive authentication "
            "methods and cannot be used together.")));
  }
  if (!opts.has<UnifiedCredentialsOption>() && !opts.has<ApiKeyOption>()) {
    opts.set<UnifiedCredentialsOption>(
        MakeGoogleDefaultCredentials(internal::MakeAuthOptions(opts)));
  }
  if (!opts.has<rest_internal::LongrunningEndpointOption>()) {
    opts.set<rest_internal::LongrunningEndpointOption>(
        "https://longrunning.googleapis.com");
  }
  if (opts.has<EndpointOption>()) {
    auto& endpoint = opts.lookup<EndpointOption>();
    if (!absl::StartsWithIgnoreCase(endpoint, "http://") &&
        !absl::StartsWithIgnoreCase(endpoint, "https://")) {
      endpoint = absl::StrCat("https://", endpoint);
    }
  }
  if (!opts.has<RestTracingOptionsOption>()) {
    opts.set<RestTracingOptionsOption>(DefaultTracingOptions());
  }
  return opts;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
