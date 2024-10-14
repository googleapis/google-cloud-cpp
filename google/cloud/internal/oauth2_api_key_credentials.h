// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_API_KEY_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_API_KEY_CREDENTIALS_H

#include "google/cloud/internal/credentials_impl.h"
#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A `Credentials` type representing a Bearer Token OAuth 2.0 credentials.
 */
class ApiKeyCredentials : public oauth2_internal::Credentials {
 public:
  explicit ApiKeyCredentials(std::string api_key);

  StatusOr<AccessToken> GetToken(
      std::chrono::system_clock::time_point tp) override;

  StatusOr<std::pair<std::string, std::string>> AuthenticationHeader(
      std::chrono::system_clock::time_point) override;

 private:
  std::string api_key_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_API_KEY_CREDENTIALS_H
