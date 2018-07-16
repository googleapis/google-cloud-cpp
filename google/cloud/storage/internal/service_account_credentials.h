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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SERVICE_ACCOUNT_CREDENTIALS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SERVICE_ACCOUNT_CREDENTIALS_H_

#include "google/cloud/storage/credentials.h"
#include "google/cloud/storage/internal/credential_constants.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <iostream>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * A C++ wrapper for Google's Service Account Credentials.
 *
 * Takes a JSON object representing the contents of a service account keyfile,
 * and uses Google's OAuth2 service to obtain an access token.
 *
 * @warning
 * The current implementation is a placeholder to unblock development of the
 * Google Cloud Storage client libraries. There is substantial work needed
 * before this class is complete, in fact, we do not even have a complete set of
 * requirements for it.
 *
 * @see
 *   https://developers.google.com/identity/protocols/OAuth2ServiceAccount
 *   https://tools.ietf.org/html/rfc7523
 *
 * @tparam HttpRequestBuilderType a dependency injection point. It makes it
 *     possible to mock the libcurl wrappers.
 * @tparam ClockType a dependency injection point to fetch the current time.
 */
template <typename HttpRequestBuilderType = CurlRequestBuilder,
          typename ClockType = std::chrono::system_clock>
class ServiceAccountCredentials : public storage::Credentials {
 public:
  explicit ServiceAccountCredentials(std::string const& content)
      : ServiceAccountCredentials(content, GoogleOAuthRefreshEndpoint()) {}

  explicit ServiceAccountCredentials(std::string const& content,
                                     std::string oauth_server)
      : expiration_time_(), clock_() {
    nl::json credentials = nl::json::parse(content);
    // Below, we construct a JWT refresh request used to obtain an access token.
    // The structure of a JWT is defined in RFC 7519 (see
    // https://tools.ietf.org/html/rfc7519), and Google-specific JWT validation
    // logic is further described at:
    // https://cloud.google.com/endpoints/docs/frameworks/java/troubleshoot-jwt
    nl::json assertion_header = {
        {"alg", "RS256"},
        {"kid", credentials["private_key_id"].get_ref<std::string const&>()},
        {"typ", "JWT"}};

    // TODO(#770): Remove all scopes except "cloud-platform".
    std::string scope = std::string(GoogleOAuthScopeCloudPlatform()) + " " +
                        GoogleOAuthScopeCloudPlatformReadOnly() + " " +
                        GoogleOAuthScopeDevstorageFullControl() + " " +
                        GoogleOAuthScopeDevstorageReadOnly() + " " +
                        GoogleOAuthScopeDevstorageReadWrite();
    // Some credential formats (e.g. gcloud's ADC file) don't contain a
    // "token_uri" attribute in the JSON object.  In this case, we try using the
    // default value. See the comments around GoogleOAuthRefreshEndpoint about
    // potential drawbacks to this approach.
    char const TOKEN_URI_KEY[] = "token_uri";
    std::string token_uri =
        credentials.value(TOKEN_URI_KEY, GoogleOAuthRefreshEndpoint());
    long int cur_time = static_cast<long int>(
        std::chrono::system_clock::to_time_t(clock_.now()));
    long int expiration_time =
        cur_time + GoogleOAuthAccessTokenLifetime().count();
    nl::json assertion_payload = {
        {"iss", credentials["client_email"].get_ref<std::string const&>()},
        {"scope", scope},
        {"aud", token_uri},
        {"iat", cur_time},
        // Resulting access token should be expire after one hour.
        {"exp", expiration_time}};

    HttpRequestBuilderType request_builder(std::move(oauth_server));
    std::string svc_acct_private_key_pem =
        credentials["private_key"].get_ref<std::string const&>();
    // This is the value of grant_type for JSON-formatted service account
    // keyfiles downloaded from Cloud Console.
    std::string payload("grant_type=");
    payload +=
        request_builder
            .MakeEscapedString("urn:ietf:params:oauth:grant-type:jwt-bearer")
            .get();
    payload += "&assertion=";
    payload += MakeJWTAssertion(assertion_header, assertion_payload,
                                svc_acct_private_key_pem);

    request_builder.AddHeader(
        "Content-Type: application/x-www-form-urlencoded");
    request_ = request_builder.BuildRequest(std::move(payload));
  }

  std::string AuthorizationHeader() override {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [this]() { return Refresh(); });
    return authorization_header_;
  }

 private:
  std::string MakeJWTAssertion(nl::json const& header, nl::json const& payload,
                               std::string const& pem_contents) {
    std::string encoded_header =
        OpenSslUtils::UrlsafeBase64Encode(header.dump());
    std::string encoded_payload =
        OpenSslUtils::UrlsafeBase64Encode(payload.dump());
    std::string encoded_signature =
        OpenSslUtils::UrlsafeBase64Encode(OpenSslUtils::SignStringWithPem(
            encoded_header + '.' + encoded_payload, pem_contents,
            JwtSigningAlgorithms::RS256));
    return encoded_header + '.' + encoded_payload + '.' + encoded_signature;
  }

  bool Refresh() {
    if (std::chrono::system_clock::now() < expiration_time_) {
      return true;
    }

    // TODO(#516) - use retry policies to refresh the credentials.
    auto response = request_.MakeRequest();
    if (200 != response.status_code) {
      return false;
    }

    nl::json access_token = nl::json::parse(response.payload);
    // Response should have the attributes "access_token", "expires_in", and
    // "token_type".
    std::string header =
        "Authorization: " +
        access_token["token_type"].get_ref<std::string const&>() + " " +
        access_token["access_token"].get_ref<std::string const&>();
    auto expires_in = std::chrono::seconds(access_token["expires_in"]);
    auto new_expiration = std::chrono::system_clock::now() + expires_in -
                          GoogleOAuthTokenExpirationSlack();
    // Do not update any state until all potential exceptions are raised.
    authorization_header_ = std::move(header);
    expiration_time_ = new_expiration;
    return true;
  }

  typename HttpRequestBuilderType::RequestType request_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::string authorization_header_;
  std::chrono::system_clock::time_point expiration_time_;
  ClockType clock_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SERVICE_ACCOUNT_CREDENTIALS_H_
