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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_SERVICE_ACCOUNT_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_SERVICE_ACCOUNT_CREDENTIALS_H

#include "google/cloud/internal/oauth2_credential_constants.h"
#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/internal/oauth2_refreshing_credentials_wrapper.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/optional.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Overrides the token uri provided by the service account credentials key
 * file.
 */
struct ServiceAccountCredentialsTokenUriOption {
  using Type = std::string;
};

/// Object to hold information used to instantiate an ServiceAccountCredentials.
struct ServiceAccountCredentialsInfo {
  std::string client_email;
  std::string private_key_id;
  std::string private_key;
  std::string token_uri;
  // If no set is supplied, a default set of scopes will be used.
  absl::optional<std::set<std::string>> scopes;
  // See https://developers.google.com/identity/protocols/OAuth2ServiceAccount.
  absl::optional<std::string> subject;
};

/// Parses the contents of a JSON keyfile into a ServiceAccountCredentialsInfo.
StatusOr<ServiceAccountCredentialsInfo> ParseServiceAccountCredentials(
    std::string const& content, std::string const& source,
    std::string const& default_token_uri = GoogleOAuthRefreshEndpoint());

/// Parses a refresh response JSON string and uses the current time to create a
/// TemporaryToken.
StatusOr<RefreshingCredentialsWrapper::TemporaryToken>
ParseServiceAccountRefreshResponse(rest_internal::RestResponse& response,
                                   std::chrono::system_clock::time_point now);

/**
 * Splits a ServiceAccountCredentialsInfo into header and payload components
 * and uses the current time to make a JWT assertion.
 *
 * @see
 * https://cloud.google.com/endpoints/docs/frameworks/java/troubleshoot-jwt
 *
 * @see https://tools.ietf.org/html/rfc7523
 */
std::pair<std::string, std::string> AssertionComponentsFromInfo(
    ServiceAccountCredentialsInfo const& info,
    std::chrono::system_clock::time_point now);

/**
 * Given a key and a JSON header and payload, creates a JWT assertion string.
 *
 * @see https://tools.ietf.org/html/rfc7519
 */
std::string MakeJWTAssertion(std::string const& header,
                             std::string const& payload,
                             std::string const& pem_contents);

/// Uses a ServiceAccountCredentialsInfo and the current time to construct a
/// JWT assertion. The assertion combined with the grant type is used to create
/// the refresh payload.
std::vector<std::pair<std::string, std::string>>
CreateServiceAccountRefreshPayload(ServiceAccountCredentialsInfo const& info,
                                   std::chrono::system_clock::time_point now);

/**
 * Make a self-signed JWT from the service account.
 *
 * [Self-signed JWTs] bypass the intermediate step of exchanging client
 * assertions for OAuth tokens. The advantages of self-signed JTWs include:
 *
 * - They are more efficient, as they require more or less the same amount of
 *   local work, and save a round-trip to the token endpoint, typically
 *   https://oauth2.googleapis.com/token.
 * - While this service is extremely reliable, removing external dependencies in
 *   the critical path almost always improves reliability.
 * - They work better in VPC-SC environments and other environments with limited
 *   Internet access.
 *
 * @warning At this time only scope-based self-signed JWTs are supported.
 *
 * [Self-signed JWTs]: https://google.aip.dev/auth/4111
 *
 * @param info the parsed service account information, see
 * `ParseServiceAccountCredentials()`
 * @param tp the current time
 * @return a bearer token for authentication.  Include this value in the
 *   `Authorization` header with the "Bearer" type.
 */
StatusOr<std::string> MakeSelfSignedJWT(
    ServiceAccountCredentialsInfo const& info,
    std::chrono::system_clock::time_point tp);

/**
 * Wrapper class for Google OAuth 2.0 service account credentials.
 *
 * Takes a ServiceAccountCredentialsInfo and obtains access tokens from the
 * Google Authorization Service as needed. Instances of this class should
 * usually be created via the convenience methods declared in
 * google/cloud/credentials.h.
 *
 * An HTTP Authorization header, with an access token as its value,
 * can be obtained by calling the AuthorizationHeader() method; if the current
 * access token is invalid or nearing expiration, this will class will first
 * obtain a new access token before returning the Authorization header string.
 *
 * @see https://developers.google.com/identity/protocols/OAuth2ServiceAccount
 * for an overview of using service accounts with Google's OAuth 2.0 system.
 *
 * @see https://cloud.google.com/storage/docs/reference/libraries for details on
 * how to obtain and get started with service account credentials.
 */
class ServiceAccountCredentials : public oauth2_internal::Credentials {
 public:
  using CurrentTimeFn =
      std::function<std::chrono::time_point<std::chrono::system_clock>()>;

  /**
   * Creates an instance of ServiceAccountCredentials.
   *
   * @param rest_client a dependency injection point. It makes it possible to
   *     mock internal REST types. This should generally not be overridden
   *     except for testing.
   * @param current_time_fn a dependency injection point to fetch the current
   *     time. This should generally not be overridden except for testing.
   */
  explicit ServiceAccountCredentials(
      ServiceAccountCredentialsInfo info, Options options = {},
      std::unique_ptr<rest_internal::RestClient> rest_client = nullptr,
      CurrentTimeFn current_time_fn = std::chrono::system_clock::now);

  /**
   * Returns a key value pair for an "Authorization" header.
   */
  StatusOr<std::pair<std::string, std::string>> AuthorizationHeader() override;

  /**
   * Create a RSA SHA256 signature of the blob using the Credential object.
   *
   * @param signing_account the desired service account which should sign
   *   @p blob. If not set, uses this object's account. If set, it must match
   *   this object's service account.
   * @param blob the string to sign. Note that sometimes the application must
   *   Base64-encode the data before signing.
   * @return the signed blob as raw bytes. An error if the @p signing_account
   *     does not match the email for the credential's account.
   */
  StatusOr<std::vector<std::uint8_t>> SignBlob(
      std::string const& signing_account,
      std::string const& blob) const override;

  std::string AccountEmail() const override { return info_.client_email; }

  std::string KeyId() const override { return info_.private_key_id; }

 private:
  StatusOr<RefreshingCredentialsWrapper::TemporaryToken> Refresh();

  ServiceAccountCredentialsInfo info_;
  CurrentTimeFn current_time_fn_;
  std::unique_ptr<rest_internal::RestClient> rest_client_;
  Options options_;
  mutable std::mutex mu_;
  RefreshingCredentialsWrapper refreshing_creds_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_SERVICE_ACCOUNT_CREDENTIALS_H
