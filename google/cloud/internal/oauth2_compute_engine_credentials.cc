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
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/oauth2_credential_constants.h"
#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/internal/rest_response.h"
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

ComputeEngineCredentials::ComputeEngineCredentials()
    : ComputeEngineCredentials("default") {}

ComputeEngineCredentials::ComputeEngineCredentials(
    std::string service_account_email, Options options,
    std::unique_ptr<rest_internal::RestClient> rest_client,
    CurrentTimeFn current_time_fn)
    : current_time_fn_(std::move(current_time_fn)),
      rest_client_(std::move(rest_client)),
      service_account_email_(std::move(service_account_email)),
      options_(std::move(options)) {
  if (!rest_client_) {
    options_.set<rest_internal::CurlFollowLocationOption>(true);
    rest_client_ = rest_internal::MakeDefaultRestClient(
        "http://" + google::cloud::internal::GceMetadataHostname(), options_);
  }
}

StatusOr<std::pair<std::string, std::string>>
ComputeEngineCredentials::AuthorizationHeader() {
  std::unique_lock<std::mutex> lock(mu_);
  return refreshing_creds_.AuthorizationHeader([this] { return Refresh(); });
}

std::string ComputeEngineCredentials::AccountEmail() const {
  std::unique_lock<std::mutex> lock(mu_);
  // Force a refresh on the account info.
  RetrieveServiceAccountInfo();
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

Status ComputeEngineCredentials::RetrieveServiceAccountInfo() const {
  auto response = DoMetadataServerGetRequest(
      "computeMetadata/v1/instance/service-accounts/" + service_account_email_ +
          "/",
      true);
  if (!response) {
    return std::move(response).status();
  }
  if ((*response)->StatusCode() >= 300) {
    return AsStatus(std::move(**response));
  }

  auto metadata = ParseMetadataServerResponse(**response);
  if (!metadata) {
    return metadata.status();
  }
  service_account_email_ = std::move(metadata->email);
  scopes_ = std::move(metadata->scopes);
  return Status();
}

StatusOr<RefreshingCredentialsWrapper::TemporaryToken>
ComputeEngineCredentials::Refresh() const {
  auto status = RetrieveServiceAccountInfo();
  if (!status.ok()) {
    return status;
  }

  auto response = DoMetadataServerGetRequest(
      "computeMetadata/v1/instance/service-accounts/" + service_account_email_ +
          "/token",
      false);
  if (!response) {
    return std::move(response).status();
  }
  if ((*response)->StatusCode() >= 300) {
    return AsStatus(std::move(**response));
  }

  return ParseComputeEngineRefreshResponse(**response, current_time_fn_());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
