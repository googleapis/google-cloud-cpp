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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_POPULATE_COMMON_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_POPULATE_COMMON_OPTIONS_H

#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Modify @p opts to have default values for common options.
 *
 * Add default values for common options, including:
 *  - `AuthorityOption`
 *  - `EndpointOption`
 *  - `TracingComponentsOption`
 *  - `UserAgentProductsOption`
 *  - `UserProjectOption`
 *
 * @param opts the current options. Any values already present in this
 *     collection are not modified.
 * @param endpoint_env_var an environment variable name used to override the
 *     default endpoint. If no `EndpointOption` is set in `opts`, **and** this
 *     environment variable is set, **and** its value is not the empty string,
 *     use the environment variable value for `EndpointOption`. This parameter
 *     is ignored if empty, which is useful when a service does not need an
 *     override.
 * @param emulator_env_var an environment variable name to override the endpoint
 *     and the default credentials. If this environment variable is set, use its
 *     value for `EndpointOption`. This parameter is ignored if empty, which is
 *     useful when a service does not have an emulator.
 * @param authority_env_var an environment variable name to override the value
 *     for `AuthorityOption`. This parameter is ignored if empty.
 * @param default_endpoint the default value for `EndpointOption` and
 *     `AuthorityOption` if none of the other mechanisms has set a value.
 *
 * @return opts with some common defaults set.
 */
Options PopulateCommonOptions(Options opts, std::string const& endpoint_env_var,
                              std::string const& emulator_env_var,
                              std::string const& authority_env_var,
                              std::string default_endpoint);

/// Compute the default value for the tracing components.
std::set<std::string> DefaultTracingComponents();

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_POPULATE_COMMON_OPTIONS_H
