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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_AUTHORIZED_USER_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_AUTHORIZED_USER_CREDENTIALS_H

#include "google/cloud/credentials.h"
#include "google/cloud/internal/oauth2_credential_constants.h"
#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/internal/oauth2_http_client_factory.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Object to hold information used to instantiate an AuthorizedUserCredentials.
struct AuthorizedUserCredentialsInfo {
  std::string client_id;
  std::string client_secret;
  std::string refresh_token;
  std::string token_uri;
};

/// Parses a user credentials JSON string into an AuthorizedUserCredentialsInfo.
StatusOr<AuthorizedUserCredentialsInfo> ParseAuthorizedUserCredentials(
    std::string const& content, std::string const& source,
    std::string const& default_token_uri = GoogleOAuthRefreshEndpoint());

/// Parses a refresh response JSON string into an access token.
StatusOr<internal::AccessToken> ParseAuthorizedUserRefreshResponse(
    rest_internal::RestResponse& response,
    std::chrono::system_clock::time_point now);

/**
 * Wrapper class for Google OAuth 2.0 user account credentials.
 *
 * Takes a service account email address or alias (e.g. "default") and uses the
 * Google Compute Engine instance's metadata server to obtain service account
 * metadata and OAuth 2.0 access tokens as needed. Instances of this class
 * should usually be created via the convenience methods declared in
 * google/cloud/credentials.h.
 *
 * An HTTP Authorization header, with an access token as its value,
 * can be obtained by calling the AuthorizationHeader() method; if the current
 * access token is invalid or nearing expiration, this will class will first
 * obtain a new access token before returning the Authorization header string.
 *
 * @see https://developers.google.com/identity/protocols/OAuth2 for an overview
 * of using user credentials with Google's OAuth 2.0 system.
 */
class AuthorizedUserCredentials : public Credentials {
 public:
  /**
   * Creates an instance of AuthorizedUserCredentials.
   *
   * @param rest_client a dependency injection point. It makes it possible to
   *     mock internal REST types. This should generally not be overridden
   *     except for testing.
   */
  explicit AuthorizedUserCredentials(AuthorizedUserCredentialsInfo info,
                                     Options options,
                                     HttpClientFactory client_factory);

  /**
   * Returns a key value pair for an "Authorization" header.
   */
  StatusOr<internal::AccessToken> GetToken(
      std::chrono::system_clock::time_point tp) override;

 private:
  AuthorizedUserCredentialsInfo info_;
  Options options_;
  HttpClientFactory client_factory_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_AUTHORIZED_USER_CREDENTIALS_H
