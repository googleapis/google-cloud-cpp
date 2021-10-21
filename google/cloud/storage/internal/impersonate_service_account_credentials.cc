// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/impersonate_service_account_credentials.h"
#include "google/cloud/storage/internal/unified_rest_credentials.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
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
    google::cloud::internal::ImpersonateServiceAccountConfig const& config)
    : ImpersonateServiceAccountCredentials(
          config, MakeMinimalIamCredentialsRestStub(
                      MapCredentials(config.base_credentials()))) {}

ImpersonateServiceAccountCredentials::ImpersonateServiceAccountCredentials(
    google::cloud::internal::ImpersonateServiceAccountConfig const& config,
    std::shared_ptr<MinimalIamCredentialsRest> stub)
    : stub_(std::move(stub)), request_(MakeRequest(config)) {}

StatusOr<std::string>
ImpersonateServiceAccountCredentials::AuthorizationHeader() {
  return AuthorizationHeader(std::chrono::system_clock::now());
}

StatusOr<std::string> ImpersonateServiceAccountCredentials::AuthorizationHeader(
    std::chrono::system_clock::time_point now) {
  std::unique_lock<std::mutex> lk(mu_);
  if (now + kUseSlack <= expiration_) return header_;
  auto response = stub_->GenerateAccessToken(request_);
  if (!response) return std::move(response).status();
  expiration_ = response->expiration;
  header_ = "Authorization: Bearer " + response->token;
  return header_;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
