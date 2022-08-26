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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_SERVICE_ACCOUNT_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_SERVICE_ACCOUNT_CREDENTIALS_H

#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/oauth2/credential_constants.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/oauth2/refreshing_credentials_wrapper.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/oauth2_service_account_credentials.h"
#include "google/cloud/internal/sha256_hash.h"
#include "google/cloud/optional.h"
#include "google/cloud/status_or.h"
#include "absl/types/optional.h"
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <iostream>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace oauth2 {
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

/**
 * Parses the contents of a P12 keyfile into a ServiceAccountCredentialsInfo.
 *
 * @warning We strongly recommend that applications use JSON keyfiles instead.
 *
 * @note Note that P12 keyfiles do not contain the `client_email` for the
 * service account, the application must obtain this through some other means
 * and provide them to the function.
 */
StatusOr<ServiceAccountCredentialsInfo> ParseServiceAccountP12File(
    std::string const& source,
    std::string const& default_token_uri = GoogleOAuthRefreshEndpoint());

/// Parses a refresh response JSON string and uses the current time to create a
/// TemporaryToken.
StatusOr<RefreshingCredentialsWrapper::TemporaryToken>
ParseServiceAccountRefreshResponse(
    storage::internal::HttpResponse const& response,
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
std::string CreateServiceAccountRefreshPayload(
    ServiceAccountCredentialsInfo const& info, std::string const& grant_type,
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

/// Return true if we need to use the OAuth path to create tokens
bool ServiceAccountUseOAuth(ServiceAccountCredentialsInfo const& info);

/**
 * Wrapper class for Google OAuth 2.0 service account credentials.
 *
 * Takes a ServiceAccountCredentialsInfo and obtains access tokens from the
 * Google Authorization Service as needed.  Instances of this class should
 * usually be created via the convenience methods declared in
 * google_credentials.h.
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
 *
 * @tparam HttpRequestBuilderType a dependency injection point. It makes it
 *     possible to mock internal libcurl wrappers. This should generally not be
 *     overridden except for testing.
 * @tparam ClockType a dependency injection point to fetch the current time.
 *     This should generally not be overridden except for testing.
 */
template <typename HttpRequestBuilderType =
              storage::internal::CurlRequestBuilder,
          typename ClockType = std::chrono::system_clock>
class ServiceAccountCredentials;

/// @copydoc ServiceAccountCredentials
template <>
class ServiceAccountCredentials<storage::internal::CurlRequestBuilder,
                                std::chrono::system_clock>
    : public Credentials {
 public:
  explicit ServiceAccountCredentials(ServiceAccountCredentialsInfo const& info)
      : ServiceAccountCredentials(std::move(info), {}) {}
  ServiceAccountCredentials(ServiceAccountCredentialsInfo const& info,
                            ChannelOptions const& options)
      : impl_(absl::make_unique<oauth2_internal::ServiceAccountCredentials>(
            oauth2_internal::ServiceAccountCredentialsInfo{
                info.client_email, info.private_key_id, info.private_key,
                info.token_uri, info.scopes, info.subject},
            Options{}.set<CARootsFilePathOption>(options.ssl_root_path()))) {}

  StatusOr<std::string> AuthorizationHeader() override {
    auto header = impl_->AuthorizationHeader();
    if (!header.ok()) return header.status();
    return header->first + ": " + header->second;
  }

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
      SigningAccount const& signing_account,
      std::string const& blob) const override {
    return impl_->SignBlob((signing_account.has_value()
                                ? signing_account.value()
                                : absl::optional<std::string>(absl::nullopt)),
                           blob);
  }

  std::string AccountEmail() const override { return impl_->AccountEmail(); }
  std::string KeyId() const override { return impl_->KeyId(); }

 private:
  std::unique_ptr<oauth2_internal::ServiceAccountCredentials> impl_;
};

/// @copydoc ServiceAccountCredentials
template <typename HttpRequestBuilderType, typename ClockType>
class ServiceAccountCredentials : public Credentials {
 public:
  explicit ServiceAccountCredentials(ServiceAccountCredentialsInfo info)
      : ServiceAccountCredentials(std::move(info), {}) {}
  ServiceAccountCredentials(ServiceAccountCredentialsInfo info,
                            ChannelOptions const& options)
      : info_(std::move(info)),
        options_(Options{}.set<CARootsFilePathOption>(options.ssl_root_path())),
        clock_() {}

  StatusOr<std::string> AuthorizationHeader() override {
    std::unique_lock<std::mutex> lock(mu_);
    return refreshing_creds_.AuthorizationHeader(clock_.now(),
                                                 [this] { return Refresh(); });
  }

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
      SigningAccount const& signing_account,
      std::string const& blob) const override {
    if (signing_account.has_value() &&
        signing_account.value() != info_.client_email) {
      return Status(StatusCode::kInvalidArgument,
                    "The current_credentials cannot sign blobs for " +
                        signing_account.value());
    }
    return internal::SignStringWithPem(blob, info_.private_key,
                                       JwtSigningAlgorithms::RS256);
  }

  std::string AccountEmail() const override { return info_.client_email; }
  std::string KeyId() const override { return info_.private_key_id; }

 private:
  bool UseOAuth() const { return ServiceAccountUseOAuth(info_); }

  StatusOr<RefreshingCredentialsWrapper::TemporaryToken> Refresh() {
    if (UseOAuth()) return RefreshOAuth();
    return RefreshSelfSigned();
  }

  StatusOr<RefreshingCredentialsWrapper::TemporaryToken> RefreshOAuth() const {
    HttpRequestBuilderType builder(
        info_.token_uri,
        storage::internal::GetDefaultCurlHandleFactory(options_));
    builder.AddHeader("Content-Type: application/x-www-form-urlencoded");
    // This is the value of grant_type for JSON-formatted service account
    // keyfiles downloaded from Cloud Console.
    std::string grant_type("grant_type=");
    grant_type +=
        builder.MakeEscapedString("urn:ietf:params:oauth:grant-type:jwt-bearer")
            .get();

    auto payload =
        CreateServiceAccountRefreshPayload(info_, grant_type, clock_.now());
    auto response = std::move(builder).BuildRequest().MakeRequest(payload);
    if (!response) return std::move(response).status();
    if (response->status_code >= 300) return AsStatus(*response);
    return ParseServiceAccountRefreshResponse(*response, clock_.now());
  }

  StatusOr<RefreshingCredentialsWrapper::TemporaryToken> RefreshSelfSigned()
      const {
    auto const tp = clock_.now();
    auto token = MakeSelfSignedJWT(info_, tp);
    if (!token) return std::move(token).status();
    return RefreshingCredentialsWrapper::TemporaryToken{
        "Authorization: Bearer " + *token,
        tp + GoogleOAuthAccessTokenLifetime()};
  }

  ServiceAccountCredentialsInfo info_;
  Options options_;
  mutable std::mutex mu_;
  RefreshingCredentialsWrapper refreshing_creds_;
  ClockType clock_;
};

}  // namespace oauth2
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_SERVICE_ACCOUNT_CREDENTIALS_H
