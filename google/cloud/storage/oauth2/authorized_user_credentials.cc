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

#include "google/cloud/storage/oauth2/authorized_user_credentials.h"
#include "google/cloud/storage/internal/nljson.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {
StatusOr<AuthorizedUserCredentialsInfo> ParseAuthorizedUserCredentials(
    std::string const& content, std::string const& source,
    std::string const& default_token_uri) {
  auto credentials =
      storage::internal::nl::json::parse(content, nullptr, false);
  if (credentials.is_discarded()) {
    return Status(
        StatusCode::kInvalidArgument,
        "Invalid AuthorizedUserCredentials, parsing failed on data from " +
            source);
  }

  std::string const client_id_key = "client_id";
  std::string const client_secret_key = "client_secret";
  std::string const refresh_token_key = "refresh_token";
  for (auto const& key :
       {client_id_key, client_secret_key, refresh_token_key}) {
    if (credentials.count(key) == 0) {
      return Status(StatusCode::kInvalidArgument,
                    "Invalid AuthorizedUserCredentials, the " +
                        std::string(key) +
                        " field is missing on data loaded from " + source);
    }
    if (credentials.value(key, "").empty()) {
      return Status(StatusCode::kInvalidArgument,
                    "Invalid AuthorizedUserCredentials, the " +
                        std::string(key) +
                        " field is empty on data loaded from " + source);
    }
  }
  return AuthorizedUserCredentialsInfo{
      credentials.value(client_id_key, ""),
      credentials.value(client_secret_key, ""),
      credentials.value(refresh_token_key, ""),
      // Some credential formats (e.g. gcloud's ADC file) don't contain a
      // "token_uri" attribute in the JSON object.  In this case, we try using
      // the default value.
      credentials.value("token_uri", default_token_uri)};
}

StatusOr<RefreshingCredentialsWrapper::TemporaryToken>
ParseAuthorizedUserRefreshResponse(
    storage::internal::HttpResponse const& response,
    std::chrono::system_clock::time_point now) {
  auto access_token =
      storage::internal::nl::json::parse(response.payload, nullptr, false);
  if (access_token.is_discarded() || access_token.count("access_token") == 0 ||
      access_token.count("expires_in") == 0 ||
      access_token.count("id_token") == 0 ||
      access_token.count("token_type") == 0) {
    auto payload =
        response.payload +
        "Could not find all required fields in response (access_token,"
        " id_token, expires_in, token_type).";
    return AsStatus(storage::internal::HttpResponse{response.status_code,
                                                    payload, response.headers});
  }
  std::string header = "Authorization: ";
  header += access_token.value("token_type", "");
  header += ' ';
  header += access_token.value("access_token", "");
  std::string new_id = access_token.value("id_token", "");
  auto expires_in =
      std::chrono::seconds(access_token.value("expires_in", int(0)));
  auto new_expiration = now + expires_in;
  return RefreshingCredentialsWrapper::TemporaryToken{std::move(header),
                                                      new_expiration};
}
}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
