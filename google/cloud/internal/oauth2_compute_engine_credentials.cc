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
#include "google/cloud/internal/compute_engine_util.h"
#include "google/cloud/internal/curl_options.h"
#include "google/cloud/internal/oauth2_credential_constants.h"
#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/internal/rest_response.h"
#include "absl/strings/str_split.h"
#include <nlohmann/json.hpp>
#include <set>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<ServiceAccountMetadata> ParseMetadataServerResponse(
    rest_internal::RestResponse& response) {
  auto payload = rest_internal::ReadAll(std::move(response).ExtractPayload());
  if (!payload.ok()) return payload.status();
  return ParseMetadataServerResponse(*payload);
}

ServiceAccountMetadata ParseMetadataServerResponse(std::string const& payload) {
  auto body = nlohmann::json::parse(payload, nullptr, false);
  // Parse the body, ignoring invalid or missing values.
  auto scopes = [&]() -> std::set<std::string> {
    if (!body.contains("scopes")) return {};
    auto const& s = body["scopes"];
    if (s.is_string()) {
      return absl::StrSplit(s.get<std::string>(), '\n', absl::SkipWhitespace());
    }
    // If we cannot parse the `scopes` field as an array of strings, we return
    // an empty set.
    if (!s.is_array()) return {};
    std::set<std::string> result;
    for (auto const& i : s) {
      if (!i.is_string()) return {};
      result.insert(i.get<std::string>());
    }
    return result;
  };
  auto email = [&]() -> std::string {
    if (!body.contains("email") || !body["email"].is_string()) return {};
    return body.value("email", "");
  };
  return ServiceAccountMetadata{scopes(), email()};
}

StatusOr<internal::AccessToken> ParseComputeEngineRefreshResponse(
    rest_internal::RestResponse& response,
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
        " expires_in, token_type) while trying to obtain an access token for"
        " compute engine credentials.";
    return Status{StatusCode::kInvalidArgument, error_payload, {}};
  }
  auto expires_in = std::chrono::seconds(access_token.value("expires_in", 0));
  auto new_expiration = now + expires_in;

  return internal::AccessToken{access_token.value("access_token", ""),
                               new_expiration};
}

ComputeEngineCredentials::ComputeEngineCredentials()
    : ComputeEngineCredentials("default") {}

ComputeEngineCredentials::ComputeEngineCredentials(
    std::string service_account_email, Options options,
    std::unique_ptr<rest_internal::RestClient> rest_client)
    : rest_client_(std::move(rest_client)),
      service_account_email_(std::move(service_account_email)),
      options_(std::move(options)) {
  if (!rest_client_) {
    options_.set<rest_internal::CurlFollowLocationOption>(true);
    rest_client_ = rest_internal::MakeDefaultRestClient(
        "http://" + google::cloud::internal::GceMetadataHostname(), options_);
  }
}

StatusOr<internal::AccessToken> ComputeEngineCredentials::GetToken(
    std::chrono::system_clock::time_point tp) {
  // Ignore failures fetching the account metadata, we can still get a token
  // using the initial `service_account_email_` value.
  auto email = RetrieveServiceAccountInfo();
  auto response = DoMetadataServerGetRequest(
      "computeMetadata/v1/instance/service-accounts/" + email + "/token",
      false);
  if (!response) return std::move(response).status();
  if ((*response)->StatusCode() >= 300) {
    return AsStatus(std::move(**response));
  }

  return ParseComputeEngineRefreshResponse(**response, tp);
}

std::string ComputeEngineCredentials::AccountEmail() const {
  std::lock_guard<std::mutex> lock(mu_);
  // Force a refresh on the account info.
  RetrieveServiceAccountInfo(lock);
  return service_account_email_;
}

std::string ComputeEngineCredentials::service_account_email() const {
  std::unique_lock<std::mutex> lock(mu_);
  return service_account_email_;
}

std::set<std::string> ComputeEngineCredentials::scopes() const {
  std::unique_lock<std::mutex> lock(mu_);
  return scopes_;
}

StatusOr<std::unique_ptr<rest_internal::RestResponse>>
ComputeEngineCredentials::DoMetadataServerGetRequest(std::string const& path,
                                                     bool recursive) const {
  rest_internal::RestRequest request;
  request.SetPath(path);
  request.AddHeader("metadata-flavor", "Google");
  if (recursive) request.AddQueryParameter("recursive", "true");
  return rest_client_->Get(request);
}

std::string ComputeEngineCredentials::RetrieveServiceAccountInfo() const {
  return RetrieveServiceAccountInfo(std::lock_guard<std::mutex>{mu_});
}

std::string ComputeEngineCredentials::RetrieveServiceAccountInfo(
    std::lock_guard<std::mutex> const&) const {
  // Fetch the metadata only once.
  if (metadata_retrieved_) return service_account_email_;

  auto response = DoMetadataServerGetRequest(
      "computeMetadata/v1/instance/service-accounts/" + service_account_email_ +
          "/",
      true);
  if (!response || (*response)->StatusCode() >= 300) {
    return service_account_email_;
  }
  auto metadata = ParseMetadataServerResponse(**response);
  if (!metadata) return service_account_email_;
  service_account_email_ = std::move(metadata->email);
  scopes_ = std::move(metadata->scopes);
  metadata_retrieved_ = true;
  return service_account_email_;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
