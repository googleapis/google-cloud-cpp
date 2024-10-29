// Copyright 2021 Google LLC
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

#include "google/cloud/internal/oauth2_impersonate_service_account_credentials.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/oauth2_credential_constants.h"
#include "google/cloud/internal/unified_rest_credentials.h"
#include "absl/strings/strip.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

GenerateAccessTokenRequest MakeRequest(
    google::cloud::internal::ImpersonateServiceAccountConfig const& config) {
  return GenerateAccessTokenRequest{
      /*.service_account=*/config.target_service_account(),
      /*.lifetime=*/config.lifetime(),
      /*.scopes=*/config.scopes(),
      /*.delegates=*/config.delegates(),
  };
}

}  // namespace

StatusOr<ImpersonatedServiceAccountCredentialsInfo>
ParseImpersonatedServiceAccountCredentials(std::string const& content,
                                           std::string const& source) {
  auto credentials = nlohmann::json::parse(content, nullptr, false);
  if (credentials.is_discarded()) {
    return internal::InvalidArgumentError(
        "Invalid ImpersonateServiceAccountCredentials, parsing failed on data "
        "from " +
            source,
        GCP_ERROR_INFO());
  }

  ImpersonatedServiceAccountCredentialsInfo info;

  auto it = credentials.find("service_account_impersonation_url");
  if (it == credentials.end()) {
    return internal::InvalidArgumentError(
        "Missing `service_account_impersonation_url` field on data from " +
            source,
        GCP_ERROR_INFO());
  }
  if (!it->is_string()) {
    return internal::InvalidArgumentError(
        "Malformed `service_account_impersonation_url` field is not a string "
        "on data from " +
            source,
        GCP_ERROR_INFO());
  }
  // We strip the service account from the path URL.
  auto url = it->get<std::string>();
  auto service_account_resource =
      absl::StripSuffix(url, ":generateAccessToken");
  auto pos = service_account_resource.rfind('/');
  if (pos == std::string::npos) {
    return internal::InvalidArgumentError(
        "Malformed `service_account_impersonation_url` field contents on data "
        "from " +
            source,
        GCP_ERROR_INFO());
  }
  info.service_account = std::string{service_account_resource.substr(pos + 1)};

  it = credentials.find("delegates");
  if (it != credentials.end()) {
    if (!it->is_array()) {
      return internal::InvalidArgumentError(
          "Malformed `delegates` field is not an array on data from " + source,
          GCP_ERROR_INFO());
    }
    for (auto const& delegate : it->items()) {
      info.delegates.push_back(delegate.value().get<std::string>());
    }
  }

  it = credentials.find("quota_project_id");
  if (it != credentials.end()) {
    if (!it->is_string()) {
      return internal::InvalidArgumentError(
          "Malformed `quota_project_id` field is not a string on data from " +
              source,
          GCP_ERROR_INFO());
    }
    info.quota_project_id = it->get<std::string>();
  }

  it = credentials.find("source_credentials");
  if (it == credentials.end()) {
    return internal::InvalidArgumentError(
        "Missing `source_credentials` field on data from " + source,
        GCP_ERROR_INFO());
  }
  if (!it->is_object()) {
    return internal::InvalidArgumentError(
        "Malformed `source_credentials` field is not an object on data from " +
            source,
        GCP_ERROR_INFO());
  }
  info.source_credentials = it->dump();

  return info;
}

ImpersonateServiceAccountCredentials::ImpersonateServiceAccountCredentials(
    google::cloud::internal::ImpersonateServiceAccountConfig const& config,
    HttpClientFactory client_factory)
    : ImpersonateServiceAccountCredentials(
          config, MakeMinimalIamCredentialsRestStub(
                      rest_internal::MapCredentials(*config.base_credentials()),
                      config.options(), std::move(client_factory))) {}

ImpersonateServiceAccountCredentials::ImpersonateServiceAccountCredentials(
    google::cloud::internal::ImpersonateServiceAccountConfig const& config,
    std::shared_ptr<MinimalIamCredentialsRest> stub)
    : stub_(std::move(stub)), request_(MakeRequest(config)) {}

StatusOr<AccessToken> ImpersonateServiceAccountCredentials::GetToken(
    std::chrono::system_clock::time_point /*tp*/) {
  return stub_->GenerateAccessToken(request_);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
