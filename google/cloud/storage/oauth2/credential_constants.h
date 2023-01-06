// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_CREDENTIAL_CONSTANTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_CREDENTIAL_CONSTANTS_H

#include "google/cloud/storage/version.h"
#include <chrono>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace oauth2 {

/**
 * Supported signing algorithms used in JWT auth flows.
 *
 * We currently only support RSA with SHA-256, but use this enum for
 * readability and easy addition of support for other algorithms.
 *
 * @deprecated Prefer using the unified credentials documented in @ref guac
 */
// NOLINTNEXTLINE(readability-identifier-naming)
enum class JwtSigningAlgorithms { RS256 };

/**
 * The max lifetime in seconds of an access token.
 *
 * @deprecated Prefer using the unified credentials documented in @ref guac
 */
constexpr std::chrono::seconds GoogleOAuthAccessTokenLifetime() {
  return std::chrono::seconds(3600);
}

/**
 * Returns the slack to consider when checking if an access token is expired.
 *
 * This time should be subtracted from a token's expiration time when checking
 * if it is expired. This prevents race conditions where, for example, one might
 * check expiration time one second before the expiration, see that the token is
 * still valid, then attempt to use it two seconds later and receive an
 * error.
 *
 * @deprecated Prefer using the unified credentials documented in @ref guac
 */
constexpr std::chrono::seconds GoogleOAuthAccessTokenExpirationSlack() {
  return std::chrono::seconds(300);
}

/**
 * The endpoint to fetch an OAuth 2.0 access token from.
 *
 * @deprecated Prefer using the unified credentials documented in @ref guac
 */
inline char const* GoogleOAuthRefreshEndpoint() {
  static constexpr char kEndpoint[] = "https://oauth2.googleapis.com/token";
  return kEndpoint;
}

/**
 * String representing the "cloud-platform" OAuth 2.0 scope.
 *
 * @deprecated Prefer using the unified credentials documented in @ref guac
 */
inline char const* GoogleOAuthScopeCloudPlatform() {
  static constexpr char kScope[] =
      "https://www.googleapis.com/auth/cloud-platform";
  return kScope;
}

}  // namespace oauth2
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_CREDENTIAL_CONSTANTS_H
