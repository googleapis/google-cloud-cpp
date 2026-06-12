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

#include "google/cloud/internal/oauth2_regional_access_boundary_token_manager.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/log.h"
#include "absl/strings/str_cat.h"
#include <array>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

auto constexpr kTokenTtl = std::chrono::seconds(6 * 3600);
auto constexpr kTtlGracePeriod = std::chrono::seconds(3600);
auto constexpr kMaximumRetryDuration = std::chrono::seconds(60);
auto constexpr kInitialBackoffDelay = std::chrono::seconds(1);
auto constexpr kMaximumBackoffDelay = std::chrono::seconds(5);
auto constexpr kBackoffScaling = 2.0;
auto constexpr kFailedLookupInitialBackoffDelay = std::chrono::seconds(15);
auto constexpr kFailedLookupMaximumBackoffDelay = std::chrono::seconds(120);
auto constexpr kFailedLookupBackoffScaling = 1.75;

}  // namespace

bool RegionalAccessBoundaryTokenManager::RetryTraits::IsPermanentFailure(
    Status const& s) {
  // Http status codes 500, 502, 503, and 504 are mapped to kUnavailable, and
  // some others that we don't mind retrying.
  return s.code() != StatusCode::kUnavailable;
}

class RegionalAccessBoundaryTokenManager::RefreshTokenLimitedTimeRetryPolicy
    : public RefreshTokenRetryPolicy {
 public:
  template <typename DurationRep, typename DurationPeriod>
  explicit RefreshTokenLimitedTimeRetryPolicy(
      std::chrono::duration<DurationRep, DurationPeriod> maximum_duration)
      : impl_(maximum_duration) {}

  RefreshTokenLimitedTimeRetryPolicy(
      RefreshTokenLimitedTimeRetryPolicy&& rhs) noexcept
      : RefreshTokenLimitedTimeRetryPolicy(rhs.maximum_duration()) {}
  RefreshTokenLimitedTimeRetryPolicy(
      RefreshTokenLimitedTimeRetryPolicy const& rhs) noexcept
      : RefreshTokenLimitedTimeRetryPolicy(rhs.maximum_duration()) {}

  std::chrono::milliseconds maximum_duration() const {
    return impl_.maximum_duration();
  }

  bool OnFailure(Status const& status) override {
    return impl_.OnFailure(status);
  }
  bool IsExhausted() const override { return impl_.IsExhausted(); }
  bool IsPermanentFailure(Status const& status) const override {
    return impl_.IsPermanentFailure(status);
  }
  std::unique_ptr<RefreshTokenRetryPolicy> clone() const override {
    return std::make_unique<RefreshTokenLimitedTimeRetryPolicy>(
        maximum_duration());
  }

  // This is provided only for backwards compatibility.
  using BaseType = RefreshTokenRetryPolicy;

 private:
  google::cloud::internal::LimitedTimeRetryPolicy<RetryTraits> impl_;
};

std::shared_ptr<RegionalAccessBoundaryTokenManager>
RegionalAccessBoundaryTokenManager::Create(std::shared_ptr<Credentials> child,
                                           HttpClientFactory client_factory,
                                           Options options) {
  auto iam_stub = MakeMinimalIamCredentialsRestStub(child, options,
                                                    std::move(client_factory));
  return std::shared_ptr<RegionalAccessBoundaryTokenManager>(
      new RegionalAccessBoundaryTokenManager(
          std::move(child), std::move(iam_stub),
          std::make_unique<
              rest_internal::AutomaticallyCreatedRestPureBackgroundThreads>(),
          std::move(options), FailedLookupBackoffPolicy,
          std::make_shared<Clock>()));
}

std::shared_ptr<RegionalAccessBoundaryTokenManager>
RegionalAccessBoundaryTokenManager::Create(
    std::shared_ptr<Credentials> child,
    std::shared_ptr<MinimalIamCredentialsRest> iam_stub, Options options) {
  return std::shared_ptr<RegionalAccessBoundaryTokenManager>(
      new RegionalAccessBoundaryTokenManager(
          std::move(child), std::move(iam_stub),
          std::make_unique<
              rest_internal::AutomaticallyCreatedRestPureBackgroundThreads>(),
          std::move(options), FailedLookupBackoffPolicy,
          std::make_shared<Clock>()));
}

std::shared_ptr<RegionalAccessBoundaryTokenManager>
RegionalAccessBoundaryTokenManager::Create(
    std::shared_ptr<Credentials> child,
    std::shared_ptr<MinimalIamCredentialsRest> iam_stub, Options options,
    std::function<std::unique_ptr<BackoffPolicy>()>
        failed_lookup_backoff_policy_fn,
    std::shared_ptr<Clock> clock, AllowedLocationsResponse allowed_locations) {
  return std::shared_ptr<RegionalAccessBoundaryTokenManager>(
      new RegionalAccessBoundaryTokenManager(
          std::move(child), std::move(iam_stub),
          std::make_unique<
              rest_internal::AutomaticallyCreatedRestPureBackgroundThreads>(),
          std::move(options), std::move(failed_lookup_backoff_policy_fn),
          std::move(clock), std::move(allowed_locations)));
}

std::unique_ptr<BackoffPolicy>
RegionalAccessBoundaryTokenManager::FailedLookupBackoffPolicy() {
  return std::make_unique<ExponentialBackoffPolicy>(
      kFailedLookupInitialBackoffDelay, kFailedLookupMaximumBackoffDelay,
      kFailedLookupBackoffScaling);
}

RegionalAccessBoundaryTokenManager::RegionalAccessBoundaryTokenManager(
    std::shared_ptr<Credentials> child,
    std::shared_ptr<MinimalIamCredentialsRest> iam_stub,
    std::unique_ptr<rest_internal::RestPureBackgroundThreads> background,
    Options options,
    std::function<std::unique_ptr<BackoffPolicy>()>
        failed_lookup_backoff_policy_fn,
    std::shared_ptr<Clock> clock, AllowedLocationsResponse allowed_locations)
    : child_(std::move(child)),
      background_(std::move(background)),
      options_(std::move(options)),
      clock_(std::move(clock)),
      retry_policy_(std::make_unique<RefreshTokenLimitedTimeRetryPolicy>(
          kMaximumRetryDuration)),
      backoff_policy_(std::make_unique<ExponentialBackoffPolicy>(
          kInitialBackoffDelay, kMaximumBackoffDelay, kBackoffScaling)),
      failed_lookup_backoff_policy_fn_(
          std::move(failed_lookup_backoff_policy_fn)),
      iam_stub_(std::move(iam_stub)),
      allowed_locations_(std::move(allowed_locations)) {
  if (!allowed_locations_.encoded_locations.empty()) {
    expire_time_ = clock_->Now() + TokenTtl();
  }
}

bool RegionalAccessBoundaryTokenManager::DoesEndpointRequireToken(
    std::string_view endpoint) {
  static constexpr std::array<std::string_view, 4> kRegionalEndpointSuffixes = {
      ".rep.googleapis.com",
      ".rep.sandbox.googleapis.com",
      ".rep.mtls.googleapis.com",
      ".rep.mtls.sandbox.googleapis.com",
  };

  return absl::EndsWithIgnoreCase(endpoint, ".googleapis.com") &&
         std::all_of(kRegionalEndpointSuffixes.begin(),
                     kRegionalEndpointSuffixes.end(),
                     [&](std::string_view suffix) {
                       return !absl::EndsWithIgnoreCase(endpoint, suffix);
                     });
}

bool RegionalAccessBoundaryTokenManager::IsTokenValid(
    std::scoped_lock<std::mutex> const&,
    std::chrono::system_clock::time_point tp) const {
  return !allowed_locations_.encoded_locations.empty() && tp < expire_time_;
}

std::chrono::seconds RegionalAccessBoundaryTokenManager::TtlGracePeriod() {
  return kTtlGracePeriod;
}

std::chrono::seconds RegionalAccessBoundaryTokenManager::TokenTtl() {
  return kTokenTtl;
}

StatusOr<rest_internal::HttpHeader>
RegionalAccessBoundaryTokenManager::AllowedLocations(
    std::chrono::system_clock::time_point tp, std::string_view endpoint) {
  auto request = child_->AllowedLocationsRequest();
  struct Visitor {
    StatusOr<rest_internal::HttpHeader> operator()(std::monostate) const {
      return rest_internal::HttpHeader{};
    }
    StatusOr<rest_internal::HttpHeader> operator()(
        ServiceAccountAllowedLocationsRequest const& r) const {
      return m.GetAllowedLocationsHeader(r, tp, endpoint);
    }
    StatusOr<rest_internal::HttpHeader> operator()(
        WorkforceIdentityAllowedLocationsRequest const& r) const {
      return m.GetAllowedLocationsHeader(r, tp, endpoint);
    }
    StatusOr<rest_internal::HttpHeader> operator()(
        WorkloadIdentityAllowedLocationsRequest const& r) const {
      return m.GetAllowedLocationsHeader(r, tp, endpoint);
    }

    RegionalAccessBoundaryTokenManager& m;
    std::chrono::system_clock::time_point tp;
    std::string_view endpoint;
  };
  return std::visit(Visitor{*this, tp, endpoint}, request);
}

StatusOr<std::vector<std::uint8_t>>
RegionalAccessBoundaryTokenManager::SignBlob(
    absl::optional<std::string> const& signing_service_account,
    std::string const& string_to_sign) const {
  return child_->SignBlob(signing_service_account, string_to_sign);
}

std::string RegionalAccessBoundaryTokenManager::AccountEmail() const {
  return child_->AccountEmail();
}

std::string RegionalAccessBoundaryTokenManager::KeyId() const {
  return child_->KeyId();
}

StatusOr<std::string> RegionalAccessBoundaryTokenManager::universe_domain()
    const {
  return child_->universe_domain();
}

StatusOr<std::string> RegionalAccessBoundaryTokenManager::universe_domain(
    google::cloud::Options const& options) const {
  return child_->universe_domain(options);
}

StatusOr<std::string> RegionalAccessBoundaryTokenManager::project_id() const {
  return child_->project_id();
}

StatusOr<std::string> RegionalAccessBoundaryTokenManager::project_id(
    Options const& options) const {
  return child_->project_id(options);
}

StatusOr<rest_internal::HttpHeader>
RegionalAccessBoundaryTokenManager::Authorization(
    std::chrono::system_clock::time_point tp) {
  return child_->Authorization(tp);
}

StatusOr<AccessToken> RegionalAccessBoundaryTokenManager::GetToken(
    std::chrono::system_clock::time_point tp) {
  return child_->GetToken(tp);
}

Credentials::AllowedLocationsRequestType
RegionalAccessBoundaryTokenManager::AllowedLocationsRequest() const {
  return child_->AllowedLocationsRequest();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
