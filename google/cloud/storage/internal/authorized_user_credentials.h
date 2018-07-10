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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_AUTHORIZED_USER_CREDENTIALS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_AUTHORIZED_USER_CREDENTIALS_H_

#include "google/cloud/storage/credentials.h"
#include "google/cloud/storage/internal/credential_constants.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/internal/nljson.h"
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
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
template <typename HttpRequestBuilderType = CurlRequestBuilder>
class AuthorizedUserCredentials : public storage::Credentials {
 public:
  explicit AuthorizedUserCredentials(std::string const& contents)
      : AuthorizedUserCredentials(contents, GoogleOAuthRefreshEndpoint()) {}

  explicit AuthorizedUserCredentials(std::string const& content,
                                     std::string oauth_server)
      : expiration_time_() {
    HttpRequestBuilderType request_builder(std::move(oauth_server));
    auto credentials = nl::json::parse(content);
    std::string payload("grant_type=refresh_token");
    payload += "&client_id=";
    payload +=
        request_builder.MakeEscapedString(credentials["client_id"]).get();
    payload += "&client_secret=";
    payload +=
        request_builder.MakeEscapedString(credentials["client_secret"]).get();
    payload += "&refresh_token=";
    payload +=
        request_builder.MakeEscapedString(credentials["refresh_token"]).get();
    request_ = request_builder.BuildRequest(std::move(payload));
  }

  std::string AuthorizationHeader() override {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [this]() { return Refresh(); });
    return authorization_header_;
  }

 private:
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
    std::string header = "Authorization: ";
    header += access_token["token_type"].get_ref<std::string const&>();
    header += ' ';
    header += access_token["access_token"].get_ref<std::string const&>();
    std::string new_id = access_token["id_token"];
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
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_AUTHORIZED_USER_CREDENTIALS_H_
