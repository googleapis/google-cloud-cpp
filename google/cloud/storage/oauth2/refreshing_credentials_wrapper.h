// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_REFRESHING_CREDENTIALS_WRAPPER_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_REFRESHING_CREDENTIALS_WRAPPER_H_

#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/storage/version.h"
#include <chrono>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {
/**
 * Wrapper for refreshable parts of a Credentials object.
 */
class RefreshingCredentialsWrapper {
 public:
  struct TemporaryToken {
    std::string token;
    std::chrono::system_clock::time_point expiration_time;
  };

  template <typename RefreshFunctor>
  StatusOr<std::string> AuthorizationHeader(
      std::chrono::system_clock::time_point now,
      RefreshFunctor refresh_fn) const {
    if (IsValid(now)) {
      return temporary_token.token;
    }

    StatusOr<TemporaryToken> new_token = refresh_fn();
    if (new_token) {
      temporary_token = *std::move(new_token);
      return temporary_token.token;
    }
    return new_token.status();
  }

  /**
   * Returns whether the current access token should be considered expired.
   *
   * When determining if a Credentials object needs to be refreshed, the IsValid
   * method should be used instead; there may be cases where a Credentials is
   * not expired but should be considered invalid.
   *
   * If a Credentials is close to expiration but not quite expired, this method
   * may still return false. This helps prevent the case where an access token
   * expires between when it is obtained and when it is used.
   */
  bool IsExpired(std::chrono::system_clock::time_point now) const;

  /**
   * Returns whether the current access token should be considered valid.
   *
   * This method should be used to determine whether a Credentials object needs
   * to be refreshed.
   */
  bool IsValid(std::chrono::system_clock::time_point now) const;

 private:
  mutable TemporaryToken temporary_token;
};

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_REFRESHING_CREDENTIALS_WRAPPER_H_
