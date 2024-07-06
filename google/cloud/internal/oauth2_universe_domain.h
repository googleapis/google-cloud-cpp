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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_UNIVERSE_DOMAIN_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_UNIVERSE_DOMAIN_H

#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <nlohmann/json.hpp>
#include <string>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Returns the value of the Google Default Universe (GDU), "googleapis.com".
std::string GoogleDefaultUniverseDomain();

/// Retrieves the value of "universe_domain" field from the json file, if it
/// exists. If the "universe_domain" field does not exist, the GDU is assumed.
/// If the "universe_domain" field exists, but is empty, an error is returned.
StatusOr<std::string> GetUniverseDomainFromCredentialsJson(
    nlohmann::json const& credentials);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_UNIVERSE_DOMAIN_H
