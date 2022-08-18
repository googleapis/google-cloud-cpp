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

#include "google/cloud/storage/oauth2/authorized_user_credentials.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace oauth2 {

StatusOr<AuthorizedUserCredentialsInfo> ParseAuthorizedUserCredentials(
    std::string const& content, std::string const& source,
    std::string const& default_token_uri) {
  auto info = google::cloud::oauth2_internal::ParseAuthorizedUserCredentials(
      content, source, default_token_uri);
  if (!info.ok()) return info.status();
  AuthorizedUserCredentialsInfo i;
  i.token_uri = std::move(info->token_uri);
  i.refresh_token = std::move(info->refresh_token);
  i.client_secret = std::move(info->client_secret);
  i.client_id = std::move(info->client_id);
  return i;
}

StatusOr<RefreshingCredentialsWrapper::TemporaryToken>
ParseAuthorizedUserRefreshResponse(
    storage::internal::HttpResponse const& response,
    std::chrono::system_clock::time_point now) {
  auto access_token = nlohmann::json::parse(response.payload, nullptr, false);
  if (!access_token.is_object() || access_token.count("access_token") == 0 ||
      access_token.count("expires_in") == 0 ||
      access_token.count("id_token") == 0 ||
      access_token.count("token_type") == 0) {
    auto payload =
        response.payload +
        "Could not find all required fields in response (access_token,"
        " id_token, expires_in, token_type) while trying to obtain an access"
        " token for authorized user credentials.";
    return AsStatus(storage::internal::HttpResponse{response.status_code,
                                                    payload, response.headers});
  }
  std::string header = "Authorization: ";
  header += access_token.value("token_type", "");
  header += ' ';
  header += access_token.value("access_token", "");
  std::string new_id = access_token.value("id_token", "");
  auto expires_in = std::chrono::seconds(access_token.value("expires_in", 0));
  auto new_expiration = now + expires_in;
  return RefreshingCredentialsWrapper::TemporaryToken{std::move(header),
                                                      new_expiration};
}

}  // namespace oauth2
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
