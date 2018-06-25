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
enum JWTSigningAlgorithms {
  RS256
};

/// The endpoint to create an access token from.
inline const char* GoogleOAuthRefreshEndpoint() {
  // TODO(houglum): Transition to using the new audience endpoint:
  //     https://oauth2.googleapis.com/token
  // The two are not always interchangeable, as some credentials require you
  // pass the same "aud" value used to create it (e.g. in a JSON keyfile
  // downloaded from the Cloud Console, this is the value for "token_uri", but
  // gcloud ADC files don't contain "token_uri", so we basically have to guess
  // which refresh endpoint, new or old, it was intended for use with.
  // Google devs may see more context on this at the internally visible issue:
  // https://issuetracker.google.com/issues/79946689
  static constexpr char endpoint[] =
      "https://accounts.google.com/o/oauth2/token";
  return endpoint;
}

/// The max lifetime of an access token, in seconds.
constexpr int GoogleOAuthAccessTokenLifetime() { return 3600; }

// OAuth2.0 scopes used for various Cloud Storage functionality.

inline const char* GoogleOAuthScopeCloudPlatform() {
  static constexpr char scope[] =
      "https://www.googleapis.com/auth/cloud-platform";
  return scope;
}

inline const char* GoogleOAuthScopeCloudPlatformReadOnly() {
  static constexpr char scope[] =
      "https://www.googleapis.com/auth/cloud-platform.read-only";
  return scope;
}

inline const char* GoogleOAuthScopeDevstorageFullControl() {
  static constexpr char scope[] =
      "https://www.googleapis.com/auth/devstorage.full_control";
  return scope;
}

inline const char* GoogleOAuthScopeDevstorageReadOnly() {
  static constexpr char scope[] =
      "https://www.googleapis.com/auth/devstorage.read_only";
  return scope;
}

inline const char* GoogleOAuthScopeDevstorageReadWrite() {
  static constexpr char scope[] =
      "https://www.googleapis.com/auth/devstorage.read_write";
  return scope;
}

/// Start refreshing tokens as soon as only this percent of their TTL is left.
constexpr int RefreshTimeSlackPercent() { return 5; }

/// Minimum time before the token expiration to start refreshing tokens.
constexpr std::chrono::seconds RefreshTimeSlackMin() {
  return std::chrono::seconds(10);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CREDENTIAL_CONSTANTS_H_
