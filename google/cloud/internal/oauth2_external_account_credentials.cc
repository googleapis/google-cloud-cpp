// Copyright 2022 Google LLC
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

#include "google/cloud/internal/oauth2_external_account_credentials.h"
#include "google/cloud/internal/external_account_parsing.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_client.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

ExternalAccountCredentials::ExternalAccountCredentials(
    ExternalAccountInfo info, HttpClientFactory client_factory, Options options)
    : info_(std::move(info)),
      client_factory_(std::move(client_factory)),
      options_(std::move(options)) {}

StatusOr<internal::AccessToken> ExternalAccountCredentials::GetToken(
    std::chrono::system_clock::time_point tp) {
  auto subject_token = (info_.token_source)(Options{});
  if (!subject_token) return std::move(subject_token).status();

  auto form_data = std::vector<std::pair<std::string, std::string>>{
      {"grant_type", "urn:ietf:params:oauth:grant-type:token-exchange"},
      {"requested_token_type", "urn:ietf:params:oauth:token-type:access_token"},
      {"scope", "https://www.googleapis.com/auth/cloud-platform"},
      {"audience", info_.audience},
      {"subject_token_type", info_.subject_token_type},
      {"subject_token", subject_token->token},
  };
  auto request = rest_internal::RestRequest(info_.token_url);

  auto client = client_factory_(options_);
  auto response = client->Post(request, form_data);
  if (!response) return std::move(response).status();
  if (MapHttpCodeToStatus((*response)->StatusCode()) != StatusCode::kOk) {
    return AsStatus(std::move(**response));
  }
  auto payload = rest_internal::ReadAll(std::move(**response).ExtractPayload());
  if (!payload) return std::move(payload.status());

  auto ec = internal::ErrorContext({
      {"audience", info_.audience},
      {"subject_token_type", info_.subject_token_type},
      {"subject_token", subject_token->token.substr(0, 32)},
      {"token_url", info_.token_url},
  });

  auto access = nlohmann::json::parse(*payload, nullptr, false);
  if (!access.is_object()) {
    return internal::InvalidArgumentError(
        "token exchange response cannot be parsed as JSON object",
        GCP_ERROR_INFO().WithContext(ec));
  }
  auto token = ValidateStringField(access, "access_token",
                                   "token-exchange-response", ec);
  if (!token) return std::move(token).status();
  auto issued_token_type = ValidateStringField(access, "issued_token_type",
                                               "token-exchange-response", ec);
  if (!issued_token_type) return std::move(issued_token_type).status();
  auto token_type =
      ValidateStringField(access, "token_type", "token-exchange-response", ec);
  if (!token_type) return std::move(token_type).status();

  if (*issued_token_type != "urn:ietf:params:oauth:token-type:access_token" ||
      *token_type != "Bearer") {
    return internal::InvalidArgumentError(
        "expected a Bearer access token in token exchange response",
        GCP_ERROR_INFO()
            .WithContext(std::move(ec))
            .WithMetadata("token_type", *token_type)
            .WithMetadata("issued_token_type", *issued_token_type));
  }
  auto it = access.find("expires_in");
  if (it == access.end() || !it->is_number_integer()) {
    return internal::InvalidArgumentError(
        "expected a numeric `expires_in` field in the token exchange response",
        GCP_ERROR_INFO()
            .WithContext(std::move(ec))
            .WithMetadata("expires_in",
                          it == access.end() ? "not-found" : it->type_name()));
  }
  return internal::AccessToken{
      *token, tp + std::chrono::seconds(it->get<std::int32_t>())};
}

StatusOr<std::pair<std::string, std::string>>
ExternalAccountCredentials::AuthorizationHeader() {
  return internal::UnimplementedError(
      "WIP(#10316) - use decorator for credentials", GCP_ERROR_INFO());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
