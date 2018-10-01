// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_CREDENTIAL_CONSTANTS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_CREDENTIAL_CONSTANTS_H_

#include "google/cloud/storage/version.h"
#include <chrono>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {

/**
 * Supported signing algorithms used in JWT auth flows.
 *
 * We currently only support RSA with SHA-256, but use this enum for
 * readability and easy addition of support for other algorithms.
 */
enum class JwtSigningAlgorithms { RS256 };

/// The max lifetime in seconds of an access token.
constexpr std::chrono::seconds GoogleOAuthAccessTokenLifetime() {
  return std::chrono::seconds(3600);
}

/**
 * The skew in seconds, to be subtracted from a token's expiration time,
 * used to determine if we should attempt to refresh and get a new access
 * token. This helps avoid a token potentially expiring mid-request.
 */
constexpr std::chrono::seconds GoogleOAuthAccessTokenExpirationSlack() {
  return std::chrono::seconds(500);
}

/// The endpoint to create an access token from.
inline char const* GoogleOAuthRefreshEndpoint() {
  static constexpr char kEndpoint[] = "https://oauth2.googleapis.com/token";
  return kEndpoint;
}

inline char const* GoogleOAuthScopeCloudPlatform() {
  static constexpr char kScope[] =
      "https://www.googleapis.com/auth/cloud-platform";
  return kScope;
}
}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_CREDENTIAL_CONSTANTS_H_
