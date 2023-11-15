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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SERVICE_ENDPOINT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SERVICE_ENDPOINT_H

#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

// If the ENDPOINT environment variable for the service or EndpointOption has a
// value, that value is used as the service endpoint. If the
// UniverseDomainOption is contained in options, then the endpoint is computed
// by replacing the googleapis.com string in the default_endpoint value with the
// value of the universe_domain, e.g.:
//  auto endpoint_option = internal::ExtractOption<EndpointOption>(opts);
//  auto endpoint = internal::DetermineServiceEndpoint(
//      internal::GetEnv("GOOGLE_CLOUD_CPP_${service_name}_SERVICE_ENDPOINT"),
//      endpoint_option, "${service}.googleapis.com", opts);
StatusOr<std::string> DetermineServiceEndpoint(
    absl::optional<std::string> endpoint_env_var,
    absl::optional<std::string> endpoint_option, std::string default_endpoint,
    Options const& options);

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SERVICE_ENDPOINT_H
