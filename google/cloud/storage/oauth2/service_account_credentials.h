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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_SERVICE_ACCOUNT_CREDENTIALS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_SERVICE_ACCOUNT_CREDENTIALS_H_

#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/oauth2/credential_constants.h"
#include "google/cloud/storage/oauth2/refreshing_credentials.h"
#include "google/cloud/storage/version.h"
#include <chrono>
#include <ctime>
#include <set>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {
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
template <typename HttpRequestBuilderType =
              storage::internal::CurlRequestBuilder,
          typename ClockType = std::chrono::system_clock>
class ServiceAccountCredentials : public RefreshingCredentials {
 public:
  explicit ServiceAccountCredentials(std::string const& json_content)
      : ServiceAccountCredentials(json_content, kGoogleOAuthRefreshEndpoint) {}

  explicit ServiceAccountCredentials(std::string const& json_content,
                                     std::string default_token_uri)
      : clock_() {
    namespace nl = storage::internal::nl;
    nl::json credentials = nl::json::parse(json_content);

    nl::json jwt_header = {
        {"alg", "RS256"},
        {"kid", credentials["private_key_id"].get_ref<std::string const&>()},
        {"typ", "JWT"}};

    long int cur_time = static_cast<long int>(
        std::chrono::system_clock::to_time_t(clock_.now()));
    // Resulting access token should be expire after one hour.
    long int expiration_time =
        cur_time + internal::GoogleOAuthAccessTokenLifetime().count();
    nl::json jwt_payload = {
        // In the event our keyfile JSON does not contain a "token_uri"
        // attribute, we fall back to the default URI.
        {"aud", credentials.value("token_uri", default_token_uri)},
        {"exp", expiration_time},
        {"iat", cur_time},
        {"iss", credentials["client_email"].get_ref<std::string const&>()},
        {"scope", std::string(kGoogleOAuthScopeCloudPlatform)}};
    std::string pem = credentials["private_key"].get_ref<std::string const&>();

    // Do not update any members until all potential exceptions are raised.
    jwt_header_ = jwt_header;
    jwt_payload_ = jwt_payload;
    signing_pem_ = pem;
  }

 protected:
  bool IsExpired() override {
    auto now = std::chrono::system_clock::now();
    return now > (expiration_time_ -
                  internal::GoogleOAuthAccessTokenExpirationSlack());
  }

  /**
   * Signs bytes using a service account's private key and the RS256 algorithm.
   *
   * @see
   *   https://tools.ietf.org/html/rfc7518#section-3.3
   */
  std::string SignBytes(std::string const& bytes) {
    return storage::internal::OpenSslUtils::SignStringWithPem(
        bytes, signing_pem_, internal::JwtSigningAlgorithms::RS256);
  }

  std::string CreateJWTAssertion(storage::internal::nl::json const& header,
                                 storage::internal::nl::json const& payload) {
    using storage::internal::OpenSslUtils;
    std::string b64_header_and_payload =
        OpenSslUtils::UrlsafeBase64Encode(header.dump()) + "." +
        OpenSslUtils::UrlsafeBase64Encode(payload.dump());
    std::string b64_signature =
        OpenSslUtils::UrlsafeBase64Encode(SignBytes(b64_header_and_payload));
    return b64_header_and_payload + "." + b64_signature;
  }

  std::string CreateRefreshPayload() {
    std::string payload("grant_type=");
    payload += internal::kGoogleOAuthJwtGrantType;
    payload += "&assertion=";
    payload += CreateJWTAssertion(jwt_header_, jwt_payload_);
    return payload;
  }

  void ParseSuccessfulRefreshResponseAndUpdateState(
      storage::internal::HttpResponse const& response)
  /* EXCLUSIVE_LOCKS_REQUIRED(mu_) */ {
    namespace nl = storage::internal::nl;
    nl::json body = nl::json::parse(response.payload);

    // Response should have the attributes "access_token", "expires_in", and
    // "token_type".
    std::string access_token =
        body["access_token"].get_ref<std::string const&>();
    auto expires_in = std::chrono::seconds(body["expires_in"]);
    std::string token_type = body["token_type"].get_ref<std::string const&>();

    // Calculate new values based on values from fields above.
    auto new_expiration = std::chrono::system_clock::now() + expires_in;

    // Do not update any members until all potential exceptions are raised.
    access_token_ = std::move(access_token);
    expiration_time_ = new_expiration;
    token_type_ = std::move(token_type);
  }

  bool Refresh() override /* EXCLUSIVE_LOCKS_REQUIRED(mu_) */ {
    // Build and make the request.
    HttpRequestBuilderType request_builder(
        jwt_payload_["aud"].get_ref<std::string const&>(),  // Refresh URI.
        storage::internal::GetDefaultCurlHandleFactory());
    request_builder.AddHeader(
        "Content-Type: application/x-www-form-urlencoded");
    // TODO(#516): Use retry policies to refresh the credentials.
    storage::internal::HttpResponse response =
        request_builder.BuildRequest().MakeRequest(CreateRefreshPayload());
    if (200 != response.status_code) {
      return false;
    }
    ParseSuccessfulRefreshResponseAndUpdateState(response);
    return true;
  }

  ClockType clock_;  // Always use a real clock unless running a unit test.
  storage::internal::nl::json jwt_header_;
  storage::internal::nl::json jwt_payload_;
  std::string signing_pem_;
};

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_SERVICE_ACCOUNT_CREDENTIALS_H_
