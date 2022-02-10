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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_POPULATE_GRPC_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_POPULATE_GRPC_OPTIONS_H

#include "google/cloud/options.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Modify @p opts to have default values for common options.
 *
 * Add default values for common options, including `UnifiedCredentialsOption`
 * `EndpointOption`,`UserAgentProductsOption`, `TracingComponentsOption`, and
 * `UserProjectOption`.
 *
 * @param opts the current options. Any values already present in this
 *     collection are not modified.
 * @param emulator_env_var an environment variable name to override the endpoint
 *     and the default credentials. If this environment variable is set, this
 *     function sets `UnifiedCredentialsOption` to the insecure credentials.
 *     Not all services have emulators, in this case, the library can provide
 *     an empty value for this environment variable.
 *
 * @return opts with some common defaults set.
 */
Options PopulateGrpcOptions(Options opts, std::string const& emulator_env_var);

/// Compute the default value for the tracing options.
TracingOptions DefaultTracingOptions();

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_POPULATE_GRPC_OPTIONS_H
