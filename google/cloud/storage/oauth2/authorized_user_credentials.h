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

#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/oauth2/credential_constants.h"
#include "google/cloud/storage/oauth2/refreshing_credentials.h"
#include <chrono>
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
 * @tparam HttpRequestBuilderType a dependency injection point. It makes it
 *     possible to mock the libcurl wrappers.
 */
template <typename HttpRequestBuilderType =
              storage::internal::CurlRequestBuilder>
class AuthorizedUserCredentials : public RefreshingCredentials {
 public:
  explicit AuthorizedUserCredentials(std::string const& json_contents)
      : AuthorizedUserCredentials(json_contents, kGoogleOAuthRefreshEndpoint) {}

  explicit AuthorizedUserCredentials(std::string const& json_contents,
                                     std::string oauth_server) {
    HttpRequestBuilderType request_builder(
        std::move(oauth_server),
        storage::internal::GetDefaultCurlHandleFactory());
    auto credentials = storage::internal::nl::json::parse(json_contents);
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
    // Because these attributes never change, we can construct the refresh
    // request's body once and reuse it.
    payload_ = std::move(payload);
    request_ = request_builder.BuildRequest();
  }

 protected:
  bool IsExpired() override {
    auto now = std::chrono::system_clock::now();
    return now > (expiration_time_ -
                  internal::GoogleOAuthAccessTokenExpirationSlack());
  }

  void ParseSuccessfulRefreshResponseAndUpdateState(
      storage::internal::HttpResponse const& response)
  /* EXCLUSIVE_LOCKS_REQUIRED(mu_) */ {
    namespace nl = storage::internal::nl;
    nl::json body = nl::json::parse(response.payload);

    // Parse all fields.
    std::string access_token =
        body["access_token"].get_ref<std::string const&>();
    auto expires_in = std::chrono::seconds(body["expires_in"]);
    // "id_token" field should be present but is unused.
    std::string token_type = body["token_type"].get_ref<std::string const&>();

    // Calculate new values based on values from fields above.
    auto new_expiration = std::chrono::system_clock::now() + expires_in -
                          internal::GoogleOAuthAccessTokenExpirationSlack();

    // Do not update any members until all potential exceptions are raised.
    access_token_ = std::move(access_token);
    expiration_time_ = new_expiration;
    token_type_ = std::move(token_type);
  }

  bool Refresh() override /* EXCLUSIVE_LOCKS_REQUIRED(mu_) */ {
    // TODO(#516): Use retry policies to refresh the credentials.
    storage::internal::HttpResponse response = request_.MakeRequest(payload_);
    if (200 != response.status_code) {
      return false;
    }
    ParseSuccessfulRefreshResponseAndUpdateState(response);
    return true;
  }

  std::string payload_;
  typename HttpRequestBuilderType::RequestType request_;
};

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_AUTHORIZED_USER_CREDENTIALS_H_
