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

#include "google/cloud/internal/oauth2_compute_engine_credentials.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
StatusOr<ServiceAccountMetadata> ParseMetadataServerResponse(
    rest_internal::RestResponse& response) {
  auto payload = rest_internal::ReadAll(std::move(response).ExtractPayload());
  if (!payload.ok()) return payload.status();
  auto response_body = nlohmann::json::parse(*payload, nullptr, false);
  // Note that the "scopes" attribute will always be present and contain a
  // JSON array. At minimum, for the request to succeed, the instance must
  // have been granted the scope that allows it to retrieve info from the
  // metadata server.
  if (response_body.is_discarded() || response_body.count("email") == 0 ||
      response_body.count("scopes") == 0) {
    auto error_payload =
        *payload +
        "Could not find all required fields in response (email, scopes).";
    return Status{StatusCode::kInvalidArgument, error_payload, {}};
  }
  ServiceAccountMetadata metadata;
  // Do not update any state until all potential errors are handled.
  metadata.email = response_body.value("email", "");
  // We need to call the .get<>() helper because the conversion is ambiguous
  // otherwise.
  metadata.scopes = response_body["scopes"].get<std::set<std::string>>();
  return metadata;
}

StatusOr<RefreshingCredentialsWrapper::TemporaryToken>
ParseComputeEngineRefreshResponse(rest_internal::RestResponse& response,
                                  std::chrono::system_clock::time_point now) {
  // Response should have the attributes "access_token", "expires_in", and
  // "token_type".
  auto payload = rest_internal::ReadAll(std::move(response).ExtractPayload());
  if (!payload.ok()) return payload.status();
  auto access_token = nlohmann::json::parse(*payload, nullptr, false);
  if (access_token.is_discarded() || access_token.count("access_token") == 0 or
      access_token.count("expires_in") == 0 or
      access_token.count("token_type") == 0) {
    auto error_payload =
        *payload +
        "Could not find all required fields in response (access_token,"
        " expires_in, token_type).";
    return Status{StatusCode::kInvalidArgument, error_payload, {}};
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

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google