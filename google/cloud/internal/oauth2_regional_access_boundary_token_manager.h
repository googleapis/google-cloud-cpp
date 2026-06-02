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
#include "google/cloud/internal/http_header.h"
#include "google/cloud/internal/oauth2_minimal_iam_credentials_rest.h"
#include "google/cloud/internal/rest_pure_background_threads_impl.h"
#include "google/cloud/internal/rest_retry_loop.h"
#include "google/cloud/internal/retry_policy_impl.h"
#include "google/cloud/log.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/strings/match.h"
#include <chrono>
#include <string>

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
    : public Credentials,
      public std::enable_shared_from_this<RegionalAccessBoundaryTokenManager> {
 public:
  using Clock = ::google::cloud::internal::SystemClock;

  struct RetryTraits {
    static bool IsPermanentFailure(Status const&);
  };

  static std::shared_ptr<RegionalAccessBoundaryTokenManager> Create(
      std::shared_ptr<Credentials> child, HttpClientFactory client_factory,
      Options options);

  static std::shared_ptr<RegionalAccessBoundaryTokenManager> Create(
      std::shared_ptr<Credentials> child,
      std::shared_ptr<MinimalIamCredentialsRest> iam_stub, Options options);

  static std::chrono::seconds TtlGracePeriod();
  static std::chrono::seconds TokenTtl();

  // Decorator overrides from Credentials that simply call the same method on
  // child_.
  StatusOr<std::vector<std::uint8_t>> SignBlob(
      absl::optional<std::string> const& signing_service_account,
      std::string const& string_to_sign) const override;
  std::string AccountEmail() const override;
  std::string KeyId() const override;
  StatusOr<std::string> universe_domain() const override;
  StatusOr<std::string> universe_domain(
      google::cloud::Options const& options) const override;
  StatusOr<std::string> project_id() const override;
  StatusOr<std::string> project_id(Options const&) const override;
  StatusOr<rest_internal::HttpHeader> Authorization(
      std::chrono::system_clock::time_point tp) override;
  StatusOr<AccessToken> GetToken(
      std::chrono::system_clock::time_point tp) override;
  Credentials::AllowedLocationsRequestType AllowedLocationsRequest()
      const override;

  // Decorator override that has an implementation.
  StatusOr<rest_internal::HttpHeader> AllowedLocations(
      std::chrono::system_clock::time_point tp,
      std::string_view endpoint) override;

  template <typename Request>
  StatusOr<rest_internal::HttpHeader> GetAllowedLocationsHeader(
      Request const& request, std::chrono::system_clock::time_point tp,
      std::string_view endpoint) {
    // If the endpoint does not need a token, return immediately.
    if (!DoesEndpointRequireToken(endpoint)) return rest_internal::HttpHeader{};
    std::scoped_lock lock(mu_);
    if (!refresh_in_progress_ && pending_refresh_.valid()) {
      pending_refresh_.get();
    }
    if (IsTokenValid(lock, tp)) {
      // Check to see if we're near expiry and if so, start refresh process.
      if (tp > (expire_time_ - TtlGracePeriod())) RefreshToken(lock, request);
      return rest_internal::HttpHeader{"x-allowed-locations",
                                       allowed_locations_.encoded_locations};
    }
    RefreshToken(lock, request);
    // Don't wait for a valid token, just return an empty header.
    return rest_internal::HttpHeader{};
  }

  // Used for testing.
  static std::shared_ptr<RegionalAccessBoundaryTokenManager> Create(
      std::shared_ptr<Credentials> child,
      std::shared_ptr<MinimalIamCredentialsRest> iam_stub, Options options,
      std::function<std::unique_ptr<BackoffPolicy>()>
          failed_lookup_backoff_policy_fn,
      std::shared_ptr<Clock> clock = std::make_shared<Clock>(),
      AllowedLocationsResponse allowed_locations = AllowedLocationsResponse{});

  // Snapshot read useful only in testing.
  bool IsOnCooldown() const {
    std::scoped_lock lock(mu_);
    return failed_lookup_cooldown_.valid() &&
           !failed_lookup_cooldown_.is_ready();
  }

  // Snapshot read useful only in testing.
  bool IsRefreshPending() const {
    std::scoped_lock lock(mu_);
    return refresh_in_progress_;
  }

 private:
  class RefreshTokenRetryPolicy : public ::google::cloud::RetryPolicy {
   public:
    virtual std::unique_ptr<RefreshTokenRetryPolicy> clone() const = 0;
  };

  class RefreshTokenLimitedTimeRetryPolicy;

  static bool DoesEndpointRequireToken(std::string_view endpoint);
  static std::unique_ptr<BackoffPolicy> FailedLookupBackoffPolicy();

  RegionalAccessBoundaryTokenManager(
      std::shared_ptr<Credentials> child,
      std::shared_ptr<MinimalIamCredentialsRest> iam_stub,
      std::unique_ptr<rest_internal::RestPureBackgroundThreads> background,
      Options options,
      std::function<std::unique_ptr<BackoffPolicy>()>
          failed_lookup_backoff_policy_fn,
      std::shared_ptr<Clock> clock = std::make_shared<Clock>(),
      AllowedLocationsResponse allowed_locations = AllowedLocationsResponse{});

  bool IsTokenValid(std::scoped_lock<std::mutex> const&,
                    std::chrono::system_clock::time_point tp) const;

  template <typename Request>
  void RefreshToken(std::scoped_lock<std::mutex> const&,
                    Request const& request) {
    if (refresh_in_progress_) return;
    if (failed_lookup_cooldown_.valid()) {
      if (!failed_lookup_cooldown_.is_ready()) return;
      (void)failed_lookup_cooldown_.get();
    }

    promise<Status> pending_refresh;
    pending_refresh_ = pending_refresh.get_future();
    auto constexpr kLocation = __func__;
    auto pending_refresh_fn = [p = std::move(pending_refresh),
                               weak = weak_from_this(), request,
                               stub = iam_stub_,
                               retry_policy = retry_policy_->clone(),
                               backoff_policy = backoff_policy_->clone(),
                               options = options_,
                               kLocation = kLocation]() mutable {
      auto refresh_attempt_fn = [stub](rest_internal::RestContext&,
                                       Options const&, Request const& request) {
        return stub->AllowedLocations(request);
      };

      StatusOr<AllowedLocationsResponse> allowed_locations =
          rest_internal::RestRetryLoop(
              *retry_policy, *backoff_policy, Idempotency::kIdempotent,
              refresh_attempt_fn, options, request, kLocation);

      if (!allowed_locations.ok()) {
        GCP_LOG(WARNING) << "AllowedLocations refresh failed with status="
                         << allowed_locations.status();
      }

      auto self = weak.lock();
      if (!self) return;
      std::scoped_lock lock(self->mu_);
      if (allowed_locations.ok()) {
        self->allowed_locations_ = *allowed_locations;
        self->expire_time_ = self->clock_->Now() + TokenTtl();
        self->failed_lookup_backoff_policy_.reset();
        p.set_value(Status{});
      } else {
        self->allowed_locations_ = AllowedLocationsResponse{};
        if (!self->failed_lookup_backoff_policy_) {
          self->failed_lookup_backoff_policy_ =
              self->failed_lookup_backoff_policy_fn_();
        }
        self->failed_lookup_cooldown_ =
            self->background_->cq().MakeRelativeTimer(
                self->failed_lookup_backoff_policy_->OnCompletion());
        p.set_value(allowed_locations.status());
      }
      self->refresh_in_progress_ = false;
    };

    refresh_in_progress_ = true;
    background_->cq().RunAsync(std::move(pending_refresh_fn));
  }

  mutable std::mutex mu_;
  std::shared_ptr<Credentials> child_;
  std::unique_ptr<rest_internal::RestPureBackgroundThreads> background_;
  Options options_;
  std::shared_ptr<Clock> clock_;
  std::unique_ptr<RefreshTokenRetryPolicy> retry_policy_;
  std::unique_ptr<BackoffPolicy> backoff_policy_;
  std::function<std::unique_ptr<BackoffPolicy>()>
      failed_lookup_backoff_policy_fn_;
  std::unique_ptr<BackoffPolicy> failed_lookup_backoff_policy_;
  std::shared_ptr<MinimalIamCredentialsRest> iam_stub_;
  future<StatusOr<std::chrono::system_clock::time_point>>
      failed_lookup_cooldown_;
  std::chrono::system_clock::time_point expire_time_;
  AllowedLocationsResponse allowed_locations_;
  future<Status> pending_refresh_;
  bool refresh_in_progress_ = false;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_REGIONAL_ACCESS_BOUNDARY_TOKEN_MANAGER_H
