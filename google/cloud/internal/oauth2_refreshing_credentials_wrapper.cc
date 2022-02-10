// Copyright 2018 Google LLC
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

#include "google/cloud/internal/oauth2_refreshing_credentials_wrapper.h"
#include "google/cloud/internal/oauth2_credential_constants.h"

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

bool RefreshingCredentialsWrapper::IsExpired(
    std::chrono::system_clock::time_point now) const {
  return now > (temporary_token_.expiration_time -
                GoogleOAuthAccessTokenExpirationSlack());
}

bool RefreshingCredentialsWrapper::IsValid(
    std::chrono::system_clock::time_point now) const {
  return !temporary_token_.token.second.empty() &&
         now <= temporary_token_.expiration_time;
}

bool RefreshingCredentialsWrapper::NeedsRefresh(
    std::chrono::system_clock::time_point now) const {
  return temporary_token_.token.second.empty() || IsExpired(now);
}

StatusOr<std::pair<std::string, std::string>>
RefreshingCredentialsWrapper::AuthorizationHeader(
    std::chrono::system_clock::time_point now,
    RefreshFunctor refresh_fn) const {
  if (!NeedsRefresh(now)) return temporary_token_.token;

  // If successful refreshing token, return it. Otherwise, return the current
  // token if it still has time left on it. If no valid token can be returned,
  // return the status of the refresh failure.
  auto new_token = refresh_fn();
  if (new_token) {
    temporary_token_ = *std::move(new_token);
    return temporary_token_.token;
  }
  if (IsValid(std::chrono::system_clock::now())) return temporary_token_.token;
  return new_token.status();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
