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

auto constexpr kUseSlack = std::chrono::seconds(30);

}  // namespace

ImpersonateServiceAccountCredentials::ImpersonateServiceAccountCredentials(
    google::cloud::internal::ImpersonateServiceAccountConfig const& config,
    std::shared_ptr<MinimalIamCredentialsRest> stub)
    : stub_(std::move(stub)), request_(MakeRequest(config)) {}

StatusOr<std::pair<std::string, std::string>>
ImpersonateServiceAccountCredentials::AuthorizationHeader() {
  return AuthorizationHeader(std::chrono::system_clock::now());
}

StatusOr<std::pair<std::string, std::string>>
ImpersonateServiceAccountCredentials::AuthorizationHeader(
    std::chrono::system_clock::time_point now) {
  std::unique_lock<std::mutex> lk(mu_);
  if (now + kUseSlack <= expiration_) return header_;
  auto response = stub_->GenerateAccessToken(request_);
  if (!response) return std::move(response).status();
  expiration_ = response->expiration;
  header_ = std::make_pair("Authorization", "Bearer " + response->token);
  return header_;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
