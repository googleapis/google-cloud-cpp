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

#include "google/cloud/access_token.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <chrono>
#include <cstdint>
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
  virtual StatusOr<AccessToken> GetToken(
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

  /// Return the universe domain from the credentials. If no explicit value is
  /// present, it is assumed to be "googleapis.com". If additional rpc calls are
  /// required, the default retry policy is used.
  virtual StatusOr<std::string> universe_domain() const;

  /// Return the universe domain from the credentials. If no explicit value is
  /// present, it is assumed to be "googleapis.com". If additional rpc calls are
  /// required, the `UniverseDomainRetryPolicyOption`, if present in the
  /// `Options`, is used. Otherwise the default retry policy is used.
  virtual StatusOr<std::string> universe_domain(
      google::cloud::Options const& options) const;

  /**
   * Return the project associated with the credentials.
   *
   * This function may return an error, for example:
   *
   * - The credential type does not have an associated project id, e.g. user
   *   credentials
   * - The credential type should have an associated project id, but it is not
   *   present, e.g., a service account key file with a missing `project_id`
   *   field.
   * - The credential type should have an associated project id, but it was
   *   not possible to retrieve it, e.g., compute engine credentials with a
   *   transient failure fetching the project id from the metadata service.
   */
  virtual StatusOr<std::string> project_id() const;

  /// @copydoc project_id()
  virtual StatusOr<std::string> project_id(Options const&) const;

  /**
   * Returns a header pair used for authentication.
   *
   * In most cases, this is the "Authorization" HTTP header. For API key
   * credentials, it is the "X-Goog-Api-Key" header.
   *
   * If unable to obtain a value for the header, which could happen for
   * `Credentials` that need to be periodically refreshed, the underlying
   * `Status` will indicate failure details from the refresh HTTP request.
   * Otherwise, the returned value will contain the header pair to be used in
   * HTTP requests.
   */
  virtual StatusOr<std::pair<std::string, std::string>> AuthenticationHeader(
      std::chrono::system_clock::time_point tp);
};

/**
 * Returns a header pair as a single string to be used for authentication.
 *
 * In most cases, this is the "Authorization" HTTP header. For API key
 * credentials, it is the "X-Goog-Api-Key" header.
 *
 * If unable to obtain a value for the header, which could happen for
 * `Credentials` that need to be periodically refreshed, the underlying `Status`
 * will indicate failure details from the refresh HTTP request. Otherwise, the
 * returned value will contain the header pair to be used in HTTP requests.
 */
StatusOr<std::string> AuthenticationHeaderJoined(
    Credentials& credentials, std::chrono::system_clock::time_point tp =
                                  std::chrono::system_clock::now());

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_CREDENTIALS_H
