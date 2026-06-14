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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_BACKGROUND_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_BACKGROUND_CREDENTIALS_H

#include "google/cloud/internal/http_header.h"
#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/internal/rest_pure_background_threads_impl.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <chrono>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// This decorator exists to manage the lifetime of any BackgroundThreads that
// may be needed by a child Credentials it decorates. This separation is
// necessary to help avoid a thread in the pool attempting to destroy the
// BackgroundThreads object, which would result in a thread attempting to join
// itself.
class BackgroundCredentials : public Credentials {
 public:
  BackgroundCredentials(
      std::unique_ptr<rest_internal::RestPureBackgroundThreads> background,
      std::shared_ptr<Credentials> child)
      : background_(std::move(background)), child_(std::move(child)) {}

  StatusOr<std::vector<std::uint8_t>> SignBlob(
      absl::optional<std::string> const& signing_service_account,
      std::string const& string_to_sign) const override {
    return child_->SignBlob(signing_service_account, string_to_sign);
  }
  std::string AccountEmail() const override { return child_->AccountEmail(); }
  std::string KeyId() const override { return child_->KeyId(); }
  StatusOr<std::string> universe_domain() const override {
    return child_->universe_domain();
  }
  StatusOr<std::string> universe_domain(
      google::cloud::Options const& options) const override {
    return child_->universe_domain(options);
  }
  StatusOr<std::string> project_id() const override {
    return child_->project_id();
  }
  StatusOr<std::string> project_id(Options const&) const override {
    return child_->project_id();
  }
  StatusOr<rest_internal::HttpHeader> Authorization(
      std::chrono::system_clock::time_point tp) override {
    return child_->Authorization(tp);
  }
  StatusOr<AccessToken> GetToken(
      std::chrono::system_clock::time_point tp) override {
    return child_->GetToken(tp);
  }
  Credentials::AllowedLocationsRequestType AllowedLocationsRequest()
      const override {
    return child_->AllowedLocationsRequest();
  }
  StatusOr<rest_internal::HttpHeader> AllowedLocations(
      std::chrono::system_clock::time_point tp,
      std::string_view endpoint) override {
    return child_->AllowedLocations(tp, endpoint);
  }

 private:
  std::unique_ptr<rest_internal::RestPureBackgroundThreads> background_;
  std::shared_ptr<Credentials> child_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_BACKGROUND_CREDENTIALS_H
