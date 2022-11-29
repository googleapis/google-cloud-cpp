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

/// Indicates whether or not to use a self-signed JWT or issue a request to
/// OAuth2.
bool ServiceAccountUseOAuth(ServiceAccountCredentialsInfo const& info);

/// Parses the contents of a P12 keyfile into a ServiceAccountCredentialsInfo.
StatusOr<ServiceAccountCredentialsInfo> ParseServiceAccountP12File(
    std::string const& source);

/// Parses the contents of a JSON keyfile into a ServiceAccountCredentialsInfo.
StatusOr<ServiceAccountCredentialsInfo> ParseServiceAccountCredentials(
    std::string const& content, std::string const& source,
    std::string const& default_token_uri = GoogleOAuthRefreshEndpoint());

/// Parses a refresh response JSON string and uses the current time to create a
/// TemporaryToken.
StatusOr<internal::AccessToken> ParseServiceAccountRefreshResponse(
    rest_internal::RestResponse& response,
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
 * Implements service account credentials for REST clients.
 *
 * This class is not intended for use by application developers. But it is
 * sufficiently complex that it deserves documentation for library developers.
 *
 * This class description assumes that you are familiar with [service accounts],
 * and [service account keys].
 *
 * Use `ParseServiceAccountCredentials()` to parse a service account key. If the
 * key is parsed successfully, you can create an instance of this class using
 * its result.
 * The service account key is never sent to Google for authentication. Instead,
 * this class creates temporary access tokens, either self-signed JWT (as
 * described in [aip/4111]), or OAuth access tokens (see [aip/4112]).
 *
 * To understand how these work it is useful to be familiar with [JWTs]. If you
 * already know what these, feel free to skip this paragraph. JWTs are
 * (relatively long) strings consisting of three (base64-encoded) components.
 * The first two are base64 encoded JSON objects. These fields in these objects
 * are often referred as "claims".  For example, the `iat` (Issued At-Time)
 * field, asserts or claims that the token was created at a certain time. The
 * third component in a JWT is a signature created using some secret. In our
 * case the signature is always created using the [RS256] signing algorithm.
 * One of the claims is always the
 * identifier for the service account key. Google Cloud has the public key
 * associated with each service account key and can use this to verify that the
 * JWT was actually signed by the service account key claimed by the JWT.
 *
 * With self-signed JWT, the token is created locally, the payload contains
 * either an audience (`"aud"`) or scope (`"scope"`) claim (but not both)
 * describing the service or services that the token grants access to. Setting
 * a more restrictive scope or audience allows applications to create tokens
 * that restrict the access for a service account. This class **only** supports
 * scope-based self-signed JWTs.
 *
 * With OAuth-based access tokens the client library creates a JWT and makes a
 * HTTP request to convert this JWT into an access token. In general,
 * self-signed JWTs are preferred over OAuth-based access tokens. On the other
 * hand, our implementation of OAuth-based access tokens has more flight hours,
 * and has been tested in more environments (on-prem, VPC-SC with different
 * restrictions, etc.).
 *
 * The `GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT` environment
 * variable can be used to prefer OAuth-based access tokens.
 *
 * Since access tokens are relatively expensive to create this class caches the
 * access tokens until they are about to expire. Use the `AuthorizationHeader()`
 * to get the current access token.
 *
 * [aip/4111]: https://google.aip.dev/auth/4111
 * [aip/4112]: https://google.aip.dev/auth/4112
 * [RS256]: https://datatracker.ietf.org/doc/html/rfc7518
 * [JWTs]: https://en.wikipedia.org/wiki/JSON_Web_Token
 * [service accounts]:
 * https://cloud.google.com/iam/docs/overview#service_account
 *
 * [iam-overview]:
 * https://cloud.google.com/iam/docs/overview
 *
 * [service account keys]:
 * https://cloud.google.com/iam/docs/creating-managing-service-account-keys#iam-service-account-keys-create-cpp
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
      absl::optional<std::string> const& signing_account,
      std::string const& blob) const override;

  std::string AccountEmail() const override { return info_.client_email; }

  std::string KeyId() const override { return info_.private_key_id; }

 private:
  bool UseOAuth();
  StatusOr<internal::AccessToken> Refresh();
  StatusOr<internal::AccessToken> RefreshOAuth() const;
  StatusOr<internal::AccessToken> RefreshSelfSigned() const;

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
