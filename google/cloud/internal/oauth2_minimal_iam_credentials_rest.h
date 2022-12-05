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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_MINIMAL_IAM_CREDENTIALS_REST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_MINIMAL_IAM_CREDENTIALS_REST_H

#include "google/cloud//version.h"
#include "google/cloud/internal/credentials_impl.h"
#include "google/cloud/internal/error_context.h"
#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include <chrono>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct GenerateAccessTokenRequest {
  std::string service_account;
  std::chrono::seconds lifetime;
  std::vector<std::string> scopes;
  std::vector<std::string> delegates;
};

/// Parse the HTTP response from a `GenerateAccessToken()` call.
StatusOr<google::cloud::internal::AccessToken> ParseGenerateAccessTokenResponse(
    rest_internal::RestResponse& response,
    google::cloud::internal::ErrorContext const& ec);

/**
 * Wrapper for IAM Credentials intended for use with
 * `ImpersonateServiceAccountCredentials`.
 */
class MinimalIamCredentialsRest {
 public:
  virtual ~MinimalIamCredentialsRest() = default;

  virtual StatusOr<google::cloud::internal::AccessToken> GenerateAccessToken(
      GenerateAccessTokenRequest const& request) = 0;
};

/**
 * Uses REST to obtain an AccessToken via IAM from the provided Credentials.
 */
class MinimalIamCredentialsRestStub : public MinimalIamCredentialsRest {
 public:
  /**
   * Creates an instance of MinimalIamCredentialsRestStub.
   *
   * @param rest_client a dependency injection point. It makes it possible to
   *     mock internal REST types. This should generally not be overridden
   *     except for testing.
   */
  MinimalIamCredentialsRestStub(
      std::shared_ptr<oauth2_internal::Credentials> credentials,
      Options options,
      std::shared_ptr<rest_internal::RestClient> rest_client = nullptr);

  StatusOr<google::cloud::internal::AccessToken> GenerateAccessToken(
      GenerateAccessTokenRequest const& request) override;

 private:
  static std::string MakeRequestPath(GenerateAccessTokenRequest const& request);

  std::shared_ptr<oauth2_internal::Credentials> credentials_;
  std::shared_ptr<rest_internal::RestClient> rest_client_;
  Options options_;
};

/**
 * Logging Decorator for use with MinimalIamCredentialsRestStub.
 */
class MinimalIamCredentialsRestLogging : public MinimalIamCredentialsRest {
 public:
  explicit MinimalIamCredentialsRestLogging(
      std::shared_ptr<MinimalIamCredentialsRest> child);

  StatusOr<google::cloud::internal::AccessToken> GenerateAccessToken(
      GenerateAccessTokenRequest const& request) override;

 private:
  std::shared_ptr<MinimalIamCredentialsRest> child_;
};

std::shared_ptr<MinimalIamCredentialsRest> MakeMinimalIamCredentialsRestStub(
    std::shared_ptr<oauth2_internal::Credentials> credentials,
    Options options = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_MINIMAL_IAM_CREDENTIALS_REST_H
