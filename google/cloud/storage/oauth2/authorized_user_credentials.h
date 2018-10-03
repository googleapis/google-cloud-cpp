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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_AUTHORIZED_USER_CREDENTIALS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_AUTHORIZED_USER_CREDENTIALS_H_

#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/oauth2/credential_constants.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {
/**
 * A C++ wrapper for Google's Authorized User Credentials.
 *
 * Takes a JSON object with the authorized user client id, secret, and access
 * token and uses Google's OAuth2 service to obtain an access token.
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
 */
template <typename HttpRequestBuilderType =
              storage::internal::CurlRequestBuilder>
class AuthorizedUserCredentials : public Credentials {
 public:
  explicit AuthorizedUserCredentials(std::string const& contents,
                                     std::string const& source)
      : AuthorizedUserCredentials(contents, source,
                                  GoogleOAuthRefreshEndpoint()) {}

  explicit AuthorizedUserCredentials(std::string const& content,
                                     std::string const& source,
                                     std::string oauth_server)
      : expiration_time_() {
    HttpRequestBuilderType request_builder(
        std::move(oauth_server),
        storage::internal::GetDefaultCurlHandleFactory());
    auto credentials =
        storage::internal::nl::json::parse(content, nullptr, false);
    if (credentials.is_null()) {
      google::cloud::internal::RaiseInvalidArgument(
          "Invalid AuthorizedUserCredentials, parsing failed on data from " +
          source);
    }
    char const CLIENT_ID_KEY[] = "client_id";
    char const CLIENT_SECRET_KEY[] = "client_secret";
    char const REFRESH_TOKEN_KEY[] = "refresh_token";
    for (auto const& key :
         {CLIENT_ID_KEY, CLIENT_SECRET_KEY, REFRESH_TOKEN_KEY}) {
      if (credentials.count(key) == 0U) {
        google::cloud::internal::RaiseInvalidArgument(
            "Invalid AuthorizedUserCredentials, the " + std::string(key) +
            " field is missing on data loaded from " + source);
      }
    }
    std::string payload("grant_type=refresh_token");
    payload += "&client_id=";
    payload +=
        request_builder.MakeEscapedString(credentials.value(CLIENT_ID_KEY, ""))
            .get();
    payload += "&client_secret=";
    payload += request_builder
                   .MakeEscapedString(credentials.value(CLIENT_SECRET_KEY, ""))
                   .get();
    payload += "&refresh_token=";
    payload += request_builder
                   .MakeEscapedString(credentials.value(REFRESH_TOKEN_KEY, ""))
                   .get();
    payload_ = std::move(payload);
    request_ = request_builder.BuildRequest();
  }

  std::string AuthorizationHeader() override {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [this]() { return Refresh(); });
    return authorization_header_;
  }

 private:
  bool Refresh() {
    namespace nl = storage::internal::nl;
    if (std::chrono::system_clock::now() < expiration_time_) {
      return true;
    }

    // TODO(#516) - use retry policies to refresh the credentials.
    auto response = request_.MakeRequest(payload_);
    if (response.status_code >= 300) {
      return false;
    }
    nl::json access_token = nl::json::parse(response.payload, nullptr, false);
    if (access_token.is_discarded() or access_token.count("token_type") == 0U or
        access_token.count("access_token") == 0U or
        access_token.count("id_token") == 0U or
        access_token.count("expires_in") == 0U) {
      return false;
    }
    std::string header = "Authorization: ";
    header += access_token.value("token_type", "");
    header += ' ';
    header += access_token.value("access_token", "");
    std::string new_id = access_token.value("id_token", "");
    auto expires_in =
        std::chrono::seconds(access_token.value("expires_in", int(0)));
    auto new_expiration = std::chrono::system_clock::now() + expires_in -
                          GoogleOAuthAccessTokenExpirationSlack();
    // Do not update any state until all potential exceptions are raised.
    authorization_header_ = std::move(header);
    expiration_time_ = new_expiration;
    return true;
  }

  typename HttpRequestBuilderType::RequestType request_;
  std::string payload_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::string authorization_header_;
  std::chrono::system_clock::time_point expiration_time_;
};

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_AUTHORIZED_USER_CREDENTIALS_H_
