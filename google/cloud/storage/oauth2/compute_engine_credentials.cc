// Copyright 2019 Google LLC
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

#include "google/cloud/storage/oauth2/compute_engine_credentials.h"
#include "google/cloud/internal/oauth2_compute_engine_credentials.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace oauth2 {

StatusOr<ServiceAccountMetadata> ParseMetadataServerResponse(
    storage::internal::HttpResponse const& response) {
  auto meta = google::cloud::oauth2_internal::ParseMetadataServerResponse(
      response.payload);
  return ServiceAccountMetadata{std::move(meta.scopes), std::move(meta.email)};
}

StatusOr<RefreshingCredentialsWrapper::TemporaryToken>
ParseComputeEngineRefreshResponse(
    storage::internal::HttpResponse const& response,
    std::chrono::system_clock::time_point now) {
  // Response should have the attributes "access_token", "expires_in", and
  // "token_type".
  auto access_token = nlohmann::json::parse(response.payload, nullptr, false);
  if (!access_token.is_object() || access_token.count("access_token") == 0 or
      access_token.count("expires_in") == 0 or
      access_token.count("token_type") == 0) {
    auto payload =
        response.payload +
        "Could not find all required fields in response (access_token,"
        " expires_in, token_type) while trying to obtain an access token for"
        " compute engine credentials.";
    return AsStatus(storage::internal::HttpResponse{response.status_code,
                                                    payload, response.headers});
  }
  std::string header = "Authorization: ";
  header += access_token.value("token_type", "");
  header += ' ';
  header += access_token.value("access_token", "");
  auto expires_in = std::chrono::seconds(access_token.value("expires_in", 0));
  auto new_expiration = now + expires_in;

  return RefreshingCredentialsWrapper::TemporaryToken{std::move(header),
                                                      new_expiration};
}

ComputeEngineCredentials<storage::internal::CurlRequestBuilder,
                         std::chrono::system_clock>::
    ComputeEngineCredentials(std::string service_account_email)
    : impl_(std::move(service_account_email), Options{}, [](Options const& o) {
        return rest_internal::MakeDefaultRestClient(std::string{}, o);
      }) {}

}  // namespace oauth2
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
