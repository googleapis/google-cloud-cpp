// Copyright 2022 Google LLC
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

#include "google/cloud/internal/oauth2_cached_credentials.h"
#include "google/cloud/internal/oauth2_credential_constants.h"

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

inline bool ExpiringSoon(internal::AccessToken const& token,
                         std::chrono::system_clock::time_point now) {
  return now + GoogleOAuthAccessTokenExpirationSlack() >= token.expiration;
}

inline bool Expired(internal::AccessToken const& token,
                    std::chrono::system_clock::time_point now) {
  return now >= token.expiration;
}

}  // namespace

CachedCredentials::CachedCredentials(std::shared_ptr<Credentials> impl)
    : impl_(std::move(impl)) {}

CachedCredentials::~CachedCredentials() = default;

StatusOr<internal::AccessToken> CachedCredentials::GetToken(
    std::chrono::system_clock::time_point now) {
  std::lock_guard<std::mutex> lk(mu_);
  if (!ExpiringSoon(token_, now)) return token_;
  auto refreshed = impl_->GetToken(now);
  if (!refreshed) {
    // Refreshing the token may have failed, but the old token may still be
    // usable
    if (Expired(token_, now)) return std::move(refreshed).status();
    return token_;
  }
  token_ = *std::move(refreshed);
  return token_;
}

StatusOr<std::vector<std::uint8_t>> CachedCredentials::SignBlob(
    absl::optional<std::string> const& signing_service_account,
    std::string const& string_to_sign) const {
  return impl_->SignBlob(signing_service_account, string_to_sign);
}

std::string CachedCredentials::AccountEmail() const {
  return impl_->AccountEmail();
}

std::string CachedCredentials::KeyId() const { return impl_->KeyId(); }

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
