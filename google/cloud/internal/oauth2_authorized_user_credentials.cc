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

#include "google/cloud/internal/oauth2_authorized_user_credentials.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<AuthorizedUserCredentialsInfo> ParseAuthorizedUserCredentials(
    std::string const& content, std::string const& source,
    std::string const& default_token_uri) {
  auto credentials = nlohmann::json::parse(content, nullptr, false);
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
ParseAuthorizedUserRefreshResponse(rest_internal::RestResponse& response,
                                   std::chrono::system_clock::time_point now) {
  auto status_code = response.StatusCode();
  auto payload = rest_internal::ReadAll(std::move(response).ExtractPayload());
  if (!payload.ok()) return std::move(payload).status();
  auto access_token = nlohmann::json::parse(*payload, nullptr, false);
  if (access_token.is_discarded() || access_token.count("access_token") == 0 ||
      access_token.count("expires_in") == 0 ||
      access_token.count("id_token") == 0 ||
      access_token.count("token_type") == 0) {
    auto error_payload =
        *payload +
        "Could not find all required fields in response (access_token,"
        " id_token, expires_in, token_type).";
    return AsStatus(status_code, error_payload);
  }
  std::string header_value = access_token.value("token_type", "");
  header_value += ' ';
  header_value += access_token.value("access_token", "");
  auto expires_in =
      std::chrono::seconds(access_token.value("expires_in", int(0)));
  auto new_expiration = now + expires_in;
  return RefreshingCredentialsWrapper::TemporaryToken{
      std::make_pair("Authorization", std::move(header_value)), new_expiration};
}

AuthorizedUserCredentials::AuthorizedUserCredentials(
    AuthorizedUserCredentialsInfo info, Options options,
    std::unique_ptr<rest_internal::RestClient> rest_client,
    CurrentTimeFn current_time_fn)
    : info_(std::move(info)),
      options_(std::move(options)),
      current_time_fn_(std::move(current_time_fn)),
      rest_client_(std::move(rest_client)) {
  if (!rest_client_) {
    rest_client_ =
        rest_internal::MakeDefaultRestClient(info_.token_uri, options_);
  }
}

StatusOr<std::pair<std::string, std::string>>
AuthorizedUserCredentials::AuthorizationHeader() {
  std::unique_lock<std::mutex> lock(mu_);
  return refreshing_creds_.AuthorizationHeader(current_time_fn_(),
                                               [this] { return Refresh(); });
}

StatusOr<RefreshingCredentialsWrapper::TemporaryToken>
AuthorizedUserCredentials::Refresh() {
  rest_internal::RestRequest request;
  request.AddHeader("content-type", "application/x-www-form-urlencoded");
  std::vector<std::pair<std::string, std::string>> form_data;
  form_data.emplace_back("grant_type", "refresh_token");
  form_data.emplace_back("client_id", info_.client_id);
  form_data.emplace_back("client_secret", info_.client_secret);
  form_data.emplace_back("refresh_token", info_.refresh_token);
  StatusOr<std::unique_ptr<rest_internal::RestResponse>> response =
      rest_client_->Post(request, form_data);
  if (!response.ok()) return std::move(response).status();
  std::unique_ptr<rest_internal::RestResponse> real_response =
      std::move(response.value());
  if (real_response->StatusCode() >= 300)
    return AsStatus(std::move(*real_response));
  return ParseAuthorizedUserRefreshResponse(*real_response, current_time_fn_());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
