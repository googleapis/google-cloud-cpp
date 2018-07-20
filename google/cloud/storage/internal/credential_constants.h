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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CREDENTIAL_CONSTANTS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CREDENTIAL_CONSTANTS_H_

#include "google/cloud/storage/version.h"
#include <chrono>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

/**
 * Supported signing algorithms used in JWT auth flows.
 *
 * We currently only support RSA with SHA-256, but use this enum for
 * readability and easy addition of support for other algorithms.
 */
enum class JwtSigningAlgorithms { RS256 };

/// The endpoint to create an access token from.
inline char const* GoogleOAuthRefreshEndpoint() {
  // TODO(#769): Transition to using the new audience endpoint:
  // https://oauth2.googleapis.com/token
  static constexpr char kEndpoint[] =
      "https://accounts.google.com/o/oauth2/token";
  return kEndpoint;
}

/// The max lifetime in seconds of an access token.
constexpr std::chrono::seconds GoogleOAuthAccessTokenLifetime() {
  return std::chrono::seconds(3600);
}

/// The skew in seconds, to be subtracted from a token's expiration time,
/// used to determine if we should attempt to refresh and get a new access
/// token. This helps avoid a token potentially expiring mid-request.
constexpr std::chrono::seconds GoogleOAuthTokenExpirationSlack() {
  return std::chrono::seconds(500);
}

//@{
/// @name OAuth2.0 scopes used for various Cloud Storage functionality.

inline char const* GoogleOAuthScopeCloudPlatform() {
  static constexpr char kScope[] =
      "https://www.googleapis.com/auth/cloud-platform";
  return kScope;
}

inline char const* GoogleOAuthScopeCloudPlatformReadOnly() {
  static constexpr char kScope[] =
      "https://www.googleapis.com/auth/cloud-platform.read-only";
  return kScope;
}

inline char const* GoogleOAuthScopeDevstorageFullControl() {
  static constexpr char kScope[] =
      "https://www.googleapis.com/auth/devstorage.full_control";
  return kScope;
}

inline char const* GoogleOAuthScopeDevstorageReadOnly() {
  static constexpr char kScope[] =
      "https://www.googleapis.com/auth/devstorage.read_only";
  return kScope;
}

inline char const* GoogleOAuthScopeDevstorageReadWrite() {
  static constexpr char kScope[] =
      "https://www.googleapis.com/auth/devstorage.read_write";
  return kScope;
}
//@}
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CREDENTIAL_CONSTANTS_H_
