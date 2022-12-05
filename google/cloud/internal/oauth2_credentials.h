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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_CREDENTIALS_H

#include "google/cloud/internal/access_token.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <chrono>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Interface for OAuth 2.0 credentials for use with Google's Unified Auth Client
 * (GUAC) library. Internally, GUAC credentials are mapped to the appropriate
 * OAuth 2.0 credential for use with GCP services with a REST API.
 *
 * Instantiating a specific kind of `Credentials` should usually be done via the
 * GUAC convenience methods declared in google/cloud/credentials.h.
 *
 * @see https://cloud.google.com/docs/authentication/ for an overview of
 * authenticating to Google Cloud Platform APIs.
 */
class Credentials {
 public:
  virtual ~Credentials() = default;

  /**
   * Obtains an access token.
   *
   * Most implementations will cache the access token and (if possible) refresh
   * the token before it expires. Refreshing the token may fail, as it often
   * requires making HTTP requests.  In that case, the last error is returned.
   *
   * @param tp the current time, most callers should provide
   *     `std::chrono::system_clock::now()`. In tests, other value may be
   *     considered.
   */
  virtual StatusOr<internal::AccessToken> GetToken(
      std::chrono::system_clock::time_point tp) = 0;

  /**
   * Try to sign @p string_to_sign using @p service_account.
   *
   * Some %Credentials types can locally sign a blob, most often just on behalf
   * of an specific service account. This function returns an error if the
   * credentials cannot sign the blob at all, or if the service account is a
   * mismatch.
   */
  virtual StatusOr<std::vector<std::uint8_t>> SignBlob(
      absl::optional<std::string> const& signing_service_account,
      std::string const& string_to_sign) const;

  /// Return the account's email associated with these credentials, if any.
  virtual std::string AccountEmail() const { return std::string{}; }

  /// Return the account's key_id associated with these credentials, if any.
  virtual std::string KeyId() const { return std::string{}; }
};

/**
 * Attempts to obtain a value for the Authorization HTTP header.
 *
 * If unable to obtain a value for the Authorization header, which could
 * happen for `Credentials` that need to be periodically refreshed, the
 * underlying `Status` will indicate failure details from the refresh HTTP
 * request. Otherwise, the returned value will contain the Authorization
 * header to be used in HTTP requests.
 */
StatusOr<std::pair<std::string, std::string>> AuthorizationHeader(
    Credentials& credentials, std::chrono::system_clock::time_point tp =
                                  std::chrono::system_clock::now());

/// @copydoc AuthorizationHeader()
StatusOr<std::string> AuthorizationHeaderJoined(
    Credentials& credentials, std::chrono::system_clock::time_point tp =
                                  std::chrono::system_clock::now());

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_CREDENTIALS_H
