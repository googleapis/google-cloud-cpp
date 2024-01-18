// Copyright 2024 Google LLC
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

#include "google/cloud/universe_domain.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/populate_common_options.h"
#include "google/cloud/internal/unified_rest_credentials.h"
#include "google/cloud/universe_domain_options.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<Options> AddUniverseDomainOption(ExperimentalTag, Options options) {
  if (!options.has<UnifiedCredentialsOption>()) {
    options.set<UnifiedCredentialsOption>(
        MakeGoogleDefaultCredentials(internal::MakeAuthOptions(options)));
  }

  auto universe_domain = GetUniverseDomain(
      ExperimentalTag{}, *options.get<UnifiedCredentialsOption>(), options);
  if (!universe_domain) return std::move(universe_domain).status();
  return options.set<internal::UniverseDomainOption>(
      *std::move(universe_domain));
}

StatusOr<std::string> GetUniverseDomain(ExperimentalTag,
                                        Credentials const& credentials,
                                        Options const& options) {
  return rest_internal::MapCredentials(credentials)->universe_domain(options);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
