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

#include "google/cloud/storage/oauth2/refreshing_credentials_wrapper.h"
#include "google/cloud/storage/oauth2/credential_constants.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {

bool RefreshingCredentialsWrapper::IsExpired(
    std::chrono::system_clock::time_point now) const {
  return now > (temporary_token.expiration_time -
                GoogleOAuthAccessTokenExpirationSlack());
}

bool RefreshingCredentialsWrapper::IsValid(
    std::chrono::system_clock::time_point now) const {
  return !temporary_token.token.empty() && !IsExpired(now);
}

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
