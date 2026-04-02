// Copyright 2026 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_REGIONAL_ACCESS_BOUNDARY_TOKEN_MANAGER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_REGIONAL_ACCESS_BOUNDARY_TOKEN_MANAGER_H

#include "google/cloud/backoff_policy.h"
#include "google/cloud/internal/clock.h"
#include "google/cloud/internal/oauth2_minimal_iam_credentials_rest.h"
#include "google/cloud/internal/rest_pure_completion_queue_impl.h"
#include "google/cloud/internal/retry_policy_impl.h"
// #include "google/cloud/access_token.h"
#include "google/cloud/internal/http_header.h"
#include "google/cloud/options.h"
// #include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/strings/match.h"
#include <chrono>
// #include <cstdint>
#include <string>
// #include <vector>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 *  Manages fetching and caching routing tokens for RAB.
 *
 *  Regional endpoints (those ending with ".rep.googleapis.com" or
 *  ".rep.sandbox.googleapis.com") do not require a routing token. Non-GDU
 *  endpoints, likewise do not require a routing token.
 *
 *  The routing tokens are refreshed asynchronously by a background thread.
 *
 *  Supported credential types:
 *    - Service Account
 *    - Impersonated Service Account
 *    - Workload Identity Federation
 *    - Workforce Identity Federation
 *    - Self Signed JWT
 *
 *  Supported environments:
 *    - GDU only
 */
class RegionalAccessBoundaryTokenManager
    : public std::enable_shared_from_this<RegionalAccessBoundaryTokenManager> {
 public:
  using Clock = ::google::cloud::internal::SystemClock;

  struct Token {
    Token() = default;
    explicit Token(std::string token) : token(std::move(token)) {}
    bool empty() const { return token.empty(); }
    std::string token;
  };

  struct RetryTraits {
    static bool IsPermanentFailure(Status const& s) {
      // Retry only on 500, 502, 503, and 504.
      return false;
    }
  };

  struct TokenRefreshRequest {};

  RegionalAccessBoundaryTokenManager(
      std::shared_ptr<oauth2_internal::Credentials> credentials,
      rest_internal::RestPureCompletionQueue cq,
      HttpClientFactory client_factory,
      std::shared_ptr<Clock> clock = std::make_shared<Clock>());

  static bool DoesEndpointRequireToken(std::string_view endpoint);

  bool IsTokenValid(std::chrono::system_clock::time_point tp) const;

  StatusOr<Token> GetServiceAccountToken(
      std::string_view sa_email, std::chrono::system_clock::time_point tp,
      std::string_view endpoint);

  void RefreshToken(std::string_view iam_path_suffix);

 private:
  std::mutex mu_;
  rest_internal::RestPureCompletionQueue cq_;
  std::shared_ptr<Clock> clock_;
  std::unique_ptr<internal::LimitedTimeRetryPolicy<RetryTraits>> retry_policy_;
  std::unique_ptr<BackoffPolicy> backoff_policy_;
  std::shared_ptr<MinimalIamCredentialsRest> iam_stub_;

  future<StatusOr<std::chrono::system_clock::time_point>>
      failed_lookup_cooldown_;
  Token token_;
  std::chrono::system_clock::time_point expire_time_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_REGIONAL_ACCESS_BOUNDARY_TOKEN_MANAGER_H
