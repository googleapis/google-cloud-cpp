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
#include "google/cloud/storage/oauth2/refreshing_credentials_wrapper.h"
#include <condition_variable>
#include <ctime>
#include <iostream>
#include <mutex>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {
/// Object to hold the result of parsing a service account JSON keyfile.
struct ServiceAccountCredentialsInfo {
  std::string private_key_id;
  std::string private_key;
  std::string token_uri;
  std::string client_email;
};

/// Parses the contents of a JSON keyfile into a ServiceAccountCredentialsInfo.
ServiceAccountCredentialsInfo ParseServiceAccountCredentials(
    std::string const& content, std::string const& source,
    std::string const& default_token_uri);

/**
 * Wrapper class for Google OAuth 2.0 service account credentials.
 *
 * Takes a string representing the JSON contents of a service account keyfile
 * and obtains access tokens from the Google Authorization Service as needed.
 * Instances of this class should usually be created via the convenience methods
 * declared in google_credentials.h.
 *
 * An HTTP Authorization header, with an access token as its value,
 * can be obtained by calling the AuthorizationHeader() method; if the current
 * access token is invalid or nearing expiration, this will class will first
 * obtain a new access token before returning the Authorization header string.

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
class ServiceAccountCredentials : public Credentials {
 public:
  explicit ServiceAccountCredentials(std::string const& content,
                                     std::string const& source)
      : ServiceAccountCredentials(content, source,
                                  GoogleOAuthRefreshEndpoint()) {}

  explicit ServiceAccountCredentials(std::string const& content,
                                     std::string const& source,
                                     std::string const& default_token_uri)
      : clock_() {
    namespace nl = storage::internal::nl;

    auto info =
        ParseServiceAccountCredentials(content, source, default_token_uri);
    auto assertion_components = std::move(AssertionComponentsFromInfo(info));

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
    payload += MakeJWTAssertion(assertion_components.first,
                                assertion_components.second, info.private_key);
    payload_ = std::move(payload);

    request_builder.AddHeader(
        "Content-Type: application/x-www-form-urlencoded");
    request_ = request_builder.BuildRequest();
    info_ = std::move(info);
  }

  StatusOr<std::string> AuthorizationHeader() override {
    std::unique_lock<std::mutex> lock(mu_);
    return refreshing_creds_.AuthorizationHeader([this] { return Refresh(); });
  }

  /**
   * Sign a blob using the credentials.
   *
   * Create a RSA SHA256 signature of the blob using the Credential object. If
   * the credentials do not support signing blobs it returns an error status.
   *
   * @param text the bytes to sign.
   * @return a Base64-encoded RSA SHA256 digest of @p blob using the current
   *   credentials.
   */
  std::pair<Status, std::string> SignString(std::string const& text) const {
    using storage::internal::OpenSslUtils;
    return std::make_pair(
        Status(), OpenSslUtils::Base64Encode(OpenSslUtils::SignStringWithPem(
                      text, info_.private_key, JwtSigningAlgorithms::RS256)));
  }

  /// Return the client id of these credentials.
  std::string client_id() const { return info_.client_email; }

 private:
  /**
   * Returns the header and payload components needed to make a JWT assertion.
   *
   * @see
   * https://cloud.google.com/endpoints/docs/frameworks/java/troubleshoot-jwt
   *
   * @see https://tools.ietf.org/html/rfc7523
   */
  std::pair<storage::internal::nl::json, storage::internal::nl::json>
  AssertionComponentsFromInfo(ServiceAccountCredentialsInfo const& info) {
    namespace nl = storage::internal::nl;
    nl::json assertion_header = {
        {"alg", "RS256"}, {"kid", info.private_key_id}, {"typ", "JWT"}};

    std::string scope = GoogleOAuthScopeCloudPlatform();

    auto now = clock_.now();
    auto expiration = now + GoogleOAuthAccessTokenLifetime();
    // As much as possible, do the time arithmetic using the std::chrono types.
    // Convert to longs only when we are dealing with timestamps since the
    // epoch.
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

    return std::make_pair(std::move(assertion_header),
                          std::move(assertion_payload));
  }

  /**
   * Given a key and a JSON header and payload, creates a JWT assertion string.
   *
   * @see https://tools.ietf.org/html/rfc7519
   */
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

  Status Refresh() {
    namespace nl = storage::internal::nl;

    auto response = request_.MakeRequest(payload_);
    if (not response.ok()) {
      return std::move(response).status();
    }
    if (response->status_code >= 300) {
      return AsStatus(*response);
    }

    nl::json access_token = nl::json::parse(response->payload, nullptr, false);
    if (access_token.is_discarded() or
        access_token.count("access_token") == 0U or
        access_token.count("expires_in") == 0U or
        access_token.count("token_type") == 0U) {
      response->payload +=
          "Could not find all required fields in response (access_token,"
          " expires_in, token_type).";
      return AsStatus(*response);
    }
    // Response should have the attributes "access_token", "expires_in", and
    // "token_type".
    std::string header =
        "Authorization: " + access_token.value("token_type", "") + " " +
        access_token.value("access_token", "");
    auto expires_in =
        std::chrono::seconds(access_token.value("expires_in", int(0)));
    auto new_expiration = std::chrono::system_clock::now() + expires_in;
    // Do not update any state until all potential exceptions are raised.
    refreshing_creds_.authorization_header = std::move(header);
    refreshing_creds_.expiration_time = new_expiration;
    return Status();
  }

  typename HttpRequestBuilderType::RequestType request_;
  std::string payload_;
  ServiceAccountCredentialsInfo info_;
  mutable std::mutex mu_;
  RefreshingCredentialsWrapper refreshing_creds_;
  ClockType clock_;
};

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_SERVICE_ACCOUNT_CREDENTIALS_H_
