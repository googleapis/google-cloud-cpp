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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_UNIVERSE_DOMAIN_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_UNIVERSE_DOMAIN_OPTIONS_H

#include "google/cloud/backoff_policy.h"
#include "google/cloud/retry_policy.h"
#include "google/cloud/version.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
// TODO(#13115): Remove internal namespace.
namespace internal {

/**
 * Use with `google::cloud::Options` to configure the universe domain used in
 * determining service endpoints.
 *
 * Consider a service with the endpoint "foo.googleapis.com" in the Google
 * Default Universe:
 *
 * @code
 * namespace gc = ::google::cloud;
 * auto conn = MakeFooConnection();
 * assert(conn->options().get<gc::EndpointOption>() == "foo.googleapis.com");
 *
 * auto options = gc::Options{}.set<gc::UniverseDomainOption>("ud.net");
 * auto conn = MakeFooConnection(options);
 * assert(conn->options().get<EndpointOption>() == "foo.ud.net");
 * @endcode
 *
 * @note Any `EndpointOption`, `AuthorityOption`, or endpoint environment
 * variable (`GOOGLE_CLOUD_CPP_<SERVICE>_ENDPOINT`) will take precedence over
 * this option. That is:
 *
 * @code
 * namespace gc = ::google::cloud;
 * auto options = gc::Options{}.set<gc::EndpointOption>("foo.googleapis.com")
 *                             .set<gc::UniverseDomainOption>("ud.net");
 * auto conn = MakeFooConnection(options);
 * assert(conn->options().get<gc::EndpointOption>() == "foo.googleapis.com");
 * @endcode
 *
 * @par Environment variable
 * This option is controlled by the `GOOGLE_CLOUD_UNIVERSE_DOMAIN` environment
 * variable. The environment variable must be set to a non-empty value.
 *
 * `EndpointOption`, `AuthorityOption`, and endpoint environment variables all
 * take precedence over `GOOGLE_CLOUD_UNIVERSE_DOMAIN`.
 */
struct UniverseDomainOption {
  using Type = std::string;
};

/**
 * The retry policy used in querying the universe domain from some credentials.
 */
class UniverseDomainRetryPolicy : public google::cloud::RetryPolicy {};

/**
 * Use with `google::cloud::Options` to configure the retry policy.
 */
struct UniverseDomainRetryPolicyOption {
  using Type = std::shared_ptr<google::cloud::RetryPolicy>;
};

/**
 * Use with `google::cloud::Options` to configure the backoff policy.
 */
struct UniverseDomainBackoffPolicyOption {
  using Type = std::shared_ptr<BackoffPolicy>;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_UNIVERSE_DOMAIN_OPTIONS_H
