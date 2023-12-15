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

#include "google/cloud/internal/oauth2_universe_domain.h"
#include "google/cloud/internal/make_status.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
auto constexpr kGoogleDefaultUniverse = "googleapis.com";
}  // namespace

std::string GoogleDefaultUniverseDomain() { return kGoogleDefaultUniverse; }

StatusOr<std::string> GetUniverseDomainFromCredentialsJson(
    nlohmann::json const& credentials) {
  auto universe_domain_iter = credentials.find("universe_domain");
  if (universe_domain_iter == credentials.end()) {
    return GoogleDefaultUniverseDomain();
  }
  if (!universe_domain_iter->is_string()) {
    return internal::InvalidArgumentError(
        "Invalid type for universe_domain field in credentials; expected "
        "string");
  }
  std::string universe_domain = *universe_domain_iter;
  if (!universe_domain.empty()) return universe_domain;
  return internal::InvalidArgumentError(
      "universe_domain field in credentials file cannot be empty");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
