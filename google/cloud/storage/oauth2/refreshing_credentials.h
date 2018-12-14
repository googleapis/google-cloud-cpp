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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_REFRESHING_CREDENTIALS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_REFRESHING_CREDENTIALS_H_

#include "google/cloud/storage/status.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include <chrono>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {
/**
 * Interface for OAuth 2.0 credentials that can be refreshed.
 */
template <class Derived>
class RefreshingCredentials : public Credentials {
 public:
  std::pair<storage::Status, std::string> AuthorizationHeader() override {
    std::unique_lock<std::mutex> lock(mu_);

    if (IsValid()) {
      return std::make_pair(storage::Status(), authorization_header_);
    }

    storage::Status status = static_cast<Derived*>(this)->Refresh();
    return std::make_pair(
        status, status.ok() ? authorization_header_ : std::string(""));
  }

 protected:
  bool IsExpired() {
    auto now = std::chrono::system_clock::now();
    return now > (expiration_time_ - GoogleOAuthAccessTokenExpirationSlack());
  }

  bool IsValid() {
    return not authorization_header_.empty() and not IsExpired();
  }

  std::string authorization_header_;
  std::chrono::system_clock::time_point expiration_time_;
  std::mutex mu_;
};

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_REFRESHING_CREDENTIALS_H_
