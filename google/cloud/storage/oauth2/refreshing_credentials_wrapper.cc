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

#include "google/cloud/storage/oauth2/refreshing_credentials_wrapper.h"
#include "google/cloud/storage/oauth2/credential_constants.h"
#include "google/cloud/internal/oauth2_refreshing_credentials_wrapper.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_split.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace oauth2 {

RefreshingCredentialsWrapper::RefreshingCredentialsWrapper()
    : impl_(
          absl::make_unique<oauth2_internal::RefreshingCredentialsWrapper>()) {}

std::pair<std::string, std::string> RefreshingCredentialsWrapper::SplitToken(
    std::string const& token) {
  std::pair<std::string, std::string> split_token = absl::StrSplit(token, ": ");
  return split_token;
}

bool RefreshingCredentialsWrapper::IsExpired(
    std::chrono::system_clock::time_point now) const {
  return now >
         (impl_->token_.expiration - GoogleOAuthAccessTokenExpirationSlack());
}

bool RefreshingCredentialsWrapper::IsValid(
    std::chrono::system_clock::time_point now) const {
  return !impl_->token_.token.empty() && !IsExpired(now);
}

}  // namespace oauth2
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
