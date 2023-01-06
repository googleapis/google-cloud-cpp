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
#include "google/cloud/internal/oauth2_credential_constants.h"
#include "google/cloud/internal/unified_rest_credentials.h"

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

ImpersonateServiceAccountCredentials::ImpersonateServiceAccountCredentials(
    google::cloud::internal::ImpersonateServiceAccountConfig const& config,
    HttpClientFactory client_factory)
    : ImpersonateServiceAccountCredentials(
          config, MakeMinimalIamCredentialsRestStub(
                      rest_internal::MapCredentials(config.base_credentials()),
                      config.options(), std::move(client_factory))) {}

ImpersonateServiceAccountCredentials::ImpersonateServiceAccountCredentials(
    google::cloud::internal::ImpersonateServiceAccountConfig const& config,
    std::shared_ptr<MinimalIamCredentialsRest> stub)
    : stub_(std::move(stub)), request_(MakeRequest(config)) {}

StatusOr<internal::AccessToken> ImpersonateServiceAccountCredentials::GetToken(
    std::chrono::system_clock::time_point /*tp*/) {
  return stub_->GenerateAccessToken(request_);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
