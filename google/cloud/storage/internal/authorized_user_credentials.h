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

#ifndef GOOGLE_CLOUD_CPP_STORAGE_CLIENT_INTERNAL_AUTHORIZED_USER_CREDENTIALS_H_
#define GOOGLE_CLOUD_CPP_STORAGE_CLIENT_INTERNAL_AUTHORIZED_USER_CREDENTIALS_H_

#include "storage/client/credentials.h"
#include "storage/client/internal/curl_request.h"
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>

namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/// The endpoint to create an access token from.
constexpr static char GOOGLE_OAUTH_REFRESH_ENDPOINT[] =
    "https://accounts.google.com/o/oauth2/token";

/// Start refreshing tokens as soon as only this percent of their TTL is left.
constexpr static auto REFRESH_TIME_SLACK_PERCENT = 5;
/// Minimum time before the token expiration to start refreshing tokens.
constexpr static auto REFRESH_TIME_SLACK_MIN = std::chrono::seconds(10);

/**
 * A C++ wrapper for Google's Authorized User Credentials.
 *
 * Takes a JSON object with the authorized user client id, secret, and access
 * token and uses Google's OAuth2 service to obtain an access token.
 *
 * @par Warning
 * The current implementation is a placeholder to unblock development of the
 * Google Cloud Storage client libraries. There is substantial work needed
 * before this class is complete, in fact, we do not even have a complete set of
 * requirements for it.
 *
 * @see
 *   https://developers.google.com/identity/protocols/OAuth2ServiceAccount
 *   https://tools.ietf.org/html/rfc7523
 *
 * @tparam HttpRequestType a dependency injection point to make HTTP requests.
 */
template <typename HttpRequestType = CurlRequest>
class AuthorizedUserCredentials : public storage::Credentials {
 public:
  explicit AuthorizedUserCredentials(std::string const& contents)
      : AuthorizedUserCredentials(contents, GOOGLE_OAUTH_REFRESH_ENDPOINT) {}

  explicit AuthorizedUserCredentials(std::string const& content,
                                     std::string oauth_server)
      : requestor_(std::move(oauth_server)), expiration_time_() {
    auto credentials = nl::json::parse(content);
    std::string payload("grant_type=refresh_token");
    payload += "&client_id=";
    payload += requestor_.MakeEscapedString(credentials["client_id"]).get();
    payload += "&client_secret=";
    payload += requestor_.MakeEscapedString(credentials["client_secret"]).get();
    payload += "&refresh_token=";
    payload += requestor_.MakeEscapedString(credentials["refresh_token"]).get();
    requestor_.PrepareRequest(std::move(payload));
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
    auto response = requestor_.MakeRequest();
    if (200 != response.status_code) {
      return false;
    }
    nl::json access_token = nl::json::parse(response.payload);
    std::string header = access_token["token_type"];
    header += ' ';
    header += access_token["access_token"].get_ref<std::string const&>();
    std::string new_id = access_token["id_token"];
    auto expires_in = std::chrono::seconds(access_token["expires_in"]);
    auto slack = expires_in * REFRESH_TIME_SLACK_PERCENT / 100;
    if (slack < REFRESH_TIME_SLACK_MIN) {
      slack = REFRESH_TIME_SLACK_MIN;
    }
    auto new_expiration = std::chrono::system_clock::now() + expires_in - slack;
    // Do not update any state until all potential exceptions are raised.
    authorization_header_ = std::move(header);
    expiration_time_ = new_expiration;
    return true;
  }

 private:
  HttpRequestType requestor_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::string authorization_header_;
  std::chrono::system_clock::time_point expiration_time_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage

#endif  // GOOGLE_CLOUD_CPP_STORAGE_CLIENT_INTERNAL_AUTHORIZED_USER_CREDENTIALS_H_
