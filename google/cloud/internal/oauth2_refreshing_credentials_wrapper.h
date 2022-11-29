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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_REFRESHING_CREDENTIALS_WRAPPER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_REFRESHING_CREDENTIALS_WRAPPER_H

#include "google/cloud/internal/access_token.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/functional/function_ref.h"
#include <chrono>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace oauth2 {
class RefreshingCredentialsWrapper;
}  // namespace oauth2
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Wrapper for refreshable parts of a Credentials object.
 */
class RefreshingCredentialsWrapper {
 public:
  using RefreshFunctor = absl::FunctionRef<StatusOr<internal::AccessToken>()>;
  using CurrentTimeFn =
      std::function<std::chrono::time_point<std::chrono::system_clock>()>;

  /**
   * Creates an instance of RefreshingCredentialsWrapper.
   *
   * @param current_time_fn a dependency injection point to fetch the current
   *     time. This should generally not be overridden except for testing.
   */
  explicit RefreshingCredentialsWrapper(
      CurrentTimeFn current_time_fn = std::chrono::system_clock::now);

  /**
   * Returns an Authorization header obtained by invoking `refresh_fn`.
   *
   * Depending on the expiration of the currently held token, `refresh_fn` may
   * or may not be called.
   */
  StatusOr<std::pair<std::string, std::string>> AuthorizationHeader(
      RefreshFunctor refresh_fn) const;

  /**
   * Returns whether the current access token should be considered valid.
   */
  bool IsValid() const;

 private:
  friend class ::google::cloud::storage::oauth2::RefreshingCredentialsWrapper;
  /**
   * Returns whether the current access token should be considered expired.
   *
   * When determining if a Credentials object needs to be refreshed, the
   * NeedsRefresh method should be used instead; there may be cases where a
   * Credentials is not expired but should be refreshed.
   *
   * If a Credentials is close to expiration but not quite expired, this method
   * may still return false. This helps prevent the case where an access token
   * expires between when it is obtained and when it is used.
   */
  bool IsExpired() const;
  bool NeedsRefresh() const;

  mutable internal::AccessToken token_;
  CurrentTimeFn current_time_fn_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_REFRESHING_CREDENTIALS_WRAPPER_H
