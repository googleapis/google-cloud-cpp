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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_IMPERSONATE_SERVICE_ACCOUNT_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_IMPERSONATE_SERVICE_ACCOUNT_CREDENTIALS_H

#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/internal/oauth2_minimal_iam_credentials_rest.h"
#include "google/cloud/version.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Provides Credentials when impersonating an existing service account.
 */
class ImpersonateServiceAccountCredentials
    : public oauth2_internal::Credentials {
 public:
  /**
   * Creates an instance of ImpersonateServiceAccountCredentials.
   *
   * @param current_time_fn a dependency injection point to fetch the current
   *     time. This should generally not be overridden except for testing.
   */
  explicit ImpersonateServiceAccountCredentials(
      google::cloud::internal::ImpersonateServiceAccountConfig const& config,
      HttpClientFactory client_factory);
  ImpersonateServiceAccountCredentials(
      google::cloud::internal::ImpersonateServiceAccountConfig const& config,
      std::shared_ptr<MinimalIamCredentialsRest> stub);

  StatusOr<internal::AccessToken> GetToken(
      std::chrono::system_clock::time_point tp) override;

 private:
  std::shared_ptr<MinimalIamCredentialsRest> stub_;
  GenerateAccessTokenRequest request_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_IMPERSONATE_SERVICE_ACCOUNT_CREDENTIALS_H
