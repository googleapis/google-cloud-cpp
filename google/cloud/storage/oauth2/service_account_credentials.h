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

#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/oauth2/credential_constants.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/status.h"
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
namespace oauth2 {
/// A plain object to hold the result of parsing a service account credentials.
struct ServiceAccountCredentialsInfo {
  std::string private_key_id;
  std::string private_key;
  std::string token_uri;
  std::string client_email;
};

/// Parse a JSON object as a ServiceAccountCredentials.
ServiceAccountCredentialsInfo ParseServiceAccountCredentials(
    std::string const& content, std::string const& source,
    std::string const& default_token_uri);

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
class ServiceAccountCredentials : public Credentials {
 public:
  explicit ServiceAccountCredentials(std::string const& content,
                                     std::string const& source)
      : ServiceAccountCredentials(content, source,
                                  GoogleOAuthRefreshEndpoint()) {}

  explicit ServiceAccountCredentials(std::string const& content,
                                     std::string const& source,
                                     std::string const& default_token_uri)
      : expiration_time_(), clock_() {
    namespace nl = storage::internal::nl;
    auto info =
        ParseServiceAccountCredentials(content, source, default_token_uri);
    // Below, we construct a JWT refresh request used to obtain an access token.
    // The structure of a JWT is defined in RFC 7519 (see
    // https://tools.ietf.org/html/rfc7519), and Google-specific JWT validation
    // logic is further described at:
    // https://cloud.google.com/endpoints/docs/frameworks/java/troubleshoot-jwt
    nl::json assertion_header = {
        {"alg", "RS256"}, {"kid", info.private_key_id}, {"typ", "JWT"}};

    std::string scope = GoogleOAuthScopeCloudPlatform();

    // As much as possible do the time arithmetic using the std::chrono types,
    // convert to longs only when we are dealing with timestamps since the
    // epoch.
    auto now = clock_.now();
    auto expiration = now + GoogleOAuthAccessTokenLifetime();
    auto now_from_epoch =
        static_cast<long>(std::chrono::system_clock::to_time_t(now));
    auto expiration_from_epoch =
        static_cast<long>(std::chrono::system_clock::to_time_t(expiration));
    nl::json assertion_payload = {
        {"iss", info.client_email},
        {"scope", scope},
        {"aud", info.token_uri},
        {"iat", now_from_epoch},
        // Resulting access token should be expire after one hour.
        {"exp", expiration_from_epoch}};

    HttpRequestBuilderType request_builder(
        std::move(info.token_uri),
        storage::internal::GetDefaultCurlHandleFactory());
    // This is the value of grant_type for JSON-formatted service account
    // keyfiles downloaded from Cloud Console.
    std::string payload("grant_type=");
    payload +=
        request_builder
            .MakeEscapedString("urn:ietf:params:oauth:grant-type:jwt-bearer")
            .get();
    payload += "&assertion=";
    payload +=
        MakeJWTAssertion(assertion_header, assertion_payload, info.private_key);
    payload_ = std::move(payload);

    request_builder.AddHeader(
        "Content-Type: application/x-www-form-urlencoded");
    request_ = request_builder.BuildRequest();
  }

  std::string AuthorizationHeader() override {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [this]() { return Refresh().ok(); });
    return authorization_header_;
  }

 private:
  std::string MakeJWTAssertion(storage::internal::nl::json const& header,
                               storage::internal::nl::json const& payload,
                               std::string const& pem_contents) {
    using storage::internal::OpenSslUtils;
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

  storage::Status Refresh() {
    namespace nl = storage::internal::nl;
    if (std::chrono::system_clock::now() < expiration_time_) {
      return storage::Status();
    }

    // TODO(#516) - use retry policies to refresh the credentials.
    auto response = request_.MakeRequest(payload_);
    if (response.status_code >= 300) {
      return storage::Status(response.status_code, std::move(response.payload));
    }

    nl::json access_token = nl::json::parse(response.payload, nullptr, false);
    if (access_token.is_discarded() or
        access_token.count("access_token") == 0U or
        access_token.count("expires_in") == 0U or
        access_token.count("token_type") == 0U) {
      return storage::Status(
          response.status_code, std::move(response.payload),
          "Could not find all required fields in response (access_token,"
          " expires_in, token_type).");
    }
    // Response should have the attributes "access_token", "expires_in", and
    // "token_type".
    std::string header =
        "Authorization: " + access_token.value("token_type", "") + " " +
        access_token.value("access_token", "");
    auto expires_in =
        std::chrono::seconds(access_token.value("expires_in", int(0)));
    auto new_expiration = std::chrono::system_clock::now() + expires_in -
                          GoogleOAuthAccessTokenExpirationSlack();
    // Do not update any state until all potential exceptions are raised.
    authorization_header_ = std::move(header);
    expiration_time_ = new_expiration;
    return storage::Status();
  }

  typename HttpRequestBuilderType::RequestType request_;
  std::string payload_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::string authorization_header_;
  std::chrono::system_clock::time_point expiration_time_;
  ClockType clock_;
};

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_SERVICE_ACCOUNT_CREDENTIALS_H_
