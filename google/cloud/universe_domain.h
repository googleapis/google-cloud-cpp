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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_UNIVERSE_DOMAIN_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_UNIVERSE_DOMAIN_H

#include "google/cloud/backoff_policy.h"
#include "google/cloud/credentials.h"
#include "google/cloud/experimental_tag.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/universe_domain_options.h"
#include "google/cloud/version.h"
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Creates an `Options` containing the `UnifiedCredentialsOption` and its
 * corresponding `UniverseDomainOption`.
 *
 * Uses the `UnifiedCredentialsOption` in `options` or
 * ApplicationDefaultCredentials if `UnifiedCredentialsOption` is not found,
 * and retrieves the universe_domain from those `Credentials` and sets the
 * `UniverseDomainOption` to the result.
 *
 * If the Metadata Server needs to be called, this function checks the options
 * for the `UniverseDomainRetryPolicyOption` and
 * `UniverseDomainBackoffPolicyOption`. If either policy option is not present a
 * default policy is used for the corresponding policy option.
 *
 * If everything succeeds, the `Options` returned contain both the
 * `UnifiedCredentialsOption` and the `UniverseDomainOption`.
 * If the `RetryPolicy` becomes exhausted or other errors are encountered, that
 * `Status` is returned.
 */
StatusOr<Options> AddUniverseDomainOption(ExperimentalTag tag,
                                          Options options = {});

/**
 * Interrogates the provided credentials for the universe_domain.
 *
 * If the Metadata Server needs to be called, this function checks the options
 * for the `UniverseDomainRetryPolicyOption` and
 * `UniverseDomainBackoffPolicyOption`. If either policy option is not present a
 * default policy is used for the corresponding policy option.
 *
 * If successful the universe_domain value is returned, otherwise a `Status`
 * indicating the error encountered is returned.
 */
StatusOr<std::string> GetUniverseDomain(ExperimentalTag tag,
                                        Credentials const& credentials,
                                        Options const& options);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_UNIVERSE_DOMAIN_H
