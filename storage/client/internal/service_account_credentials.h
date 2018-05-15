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

#ifndef GOOGLE_CLOUD_CPP_STORAGE_CLIENT_INTERNAL_SERVICE_ACCOUNT_CREDENTIALS_H_
#define GOOGLE_CLOUD_CPP_STORAGE_CLIENT_INTERNAL_SERVICE_ACCOUNT_CREDENTIALS_H_

#include "storage/client/credentials.h"
#include "storage/client/internal/curl_request.h"
#include <chrono>
#include <iostream>
#include <string>

namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

constexpr static char GOOGLE_OAUTH_REFRESH_ENDPOINT[] =
    "https://accounts.google.com/o/oauth2/token";

/**
 * A C++ wrapper for Google Service Account Credentials.
 *
 * This class parses the contents of a Google Service Account credentials file,
 * and creates a credentials object from those contents. It automatically
 * handles refreshing the credentials when needed, as well as creating the
 * appropriate header for authorization.
 *
 * @see
 *   https://developers.google.com/identity/protocols/OAuth2ServiceAccount
 *   https://tools.ietf.org/html/rfc7523
 *
 * @tparam HttpRequestType a dependency injection point to make HTTP requests.
 */
template <typename HttpRequestType = CurlRequest>
class ServiceAccountCredentials : public storage::Credentials {
 public:
  explicit ServiceAccountCredentials(std::string token)
      : ServiceAccountCredentials(std::move(token),
                                  GOOGLE_OAUTH_REFRESH_ENDPOINT) {}

  explicit ServiceAccountCredentials(std::string token,
                                     std::string oauth_server)
      : refresh_token_(std::move(token)),
        requestor_(std::move(oauth_server)),
        expiration_time_() {
    auto refresh = nl::json::parse(refresh_token_);
    std::string payload("grant_type=refresh_token");
    payload += "&client_id=";
    payload += requestor_.MakeEscapedString(refresh["client_id"]).get();
    payload += "&client_secret=";
    payload += requestor_.MakeEscapedString(refresh["client_secret"]).get();
    payload += "&refresh_token=";
    payload += requestor_.MakeEscapedString(refresh["refresh_token"]).get();
    requestor_.PrepareRequest(std::move(payload));
    Refresh();
  }

  std::string const& AuthorizationHeader() const override {
    Refresh();
    return authorization_header_;
  }

 private:
  void Refresh() const {
    auto slack = std::chrono::minutes(5);
    if (std::chrono::system_clock::now() + slack < expiration_time_) {
      return;
    }

    auto response = requestor_.MakeRequest();
    nl::json access_token = nl::json::parse(response);
    std::string header = access_token["token_type"];
    header += ' ';
    header += access_token["access_token"];
    std::string new_id = access_token["id_token"];
    auto new_expiration = std::chrono::system_clock::now() +
                          std::chrono::seconds(access_token["expires_in"]);

    authorization_header_ = std::move(header);
    id_token_ = std::move(new_id);
    expiration_time_ = new_expiration;
  }

 private:
  std::string refresh_token_;
  mutable HttpRequestType requestor_;
  mutable std::string authorization_header_;
  mutable std::string id_token_;
  mutable std::chrono::system_clock::time_point expiration_time_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage

#endif  // GOOGLE_CLOUD_CPP_STORAGE_CLIENT_SERVICE_ACCOUNT_CREDENTIALS_H_
