// Copyright 2021 Google LLC
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

#include "google/cloud/credentials.h"
#include "google/cloud/internal/credentials_impl.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

Credentials::~Credentials() = default;

std::shared_ptr<Credentials> MakeInsecureCredentials(Options opts) {
  return std::make_shared<internal::InsecureCredentialsConfig>(std::move(opts));
}

std::shared_ptr<Credentials> MakeGoogleDefaultCredentials(Options opts) {
  return std::make_shared<internal::GoogleDefaultCredentialsConfig>(
      std::move(opts));
}

std::shared_ptr<Credentials> MakeAccessTokenCredentials(
    std::string const& access_token,
    std::chrono::system_clock::time_point expiration, Options opts) {
  return std::make_shared<internal::AccessTokenConfig>(access_token, expiration,
                                                       std::move(opts));
}

std::shared_ptr<Credentials> MakeImpersonateServiceAccountCredentials(
    std::shared_ptr<Credentials> base_credentials,
    std::string target_service_account, Options opts) {
  return std::make_shared<internal::ImpersonateServiceAccountConfig>(
      std::move(base_credentials), std::move(target_service_account),
      std::move(opts));
}

std::shared_ptr<Credentials> MakeServiceAccountCredentials(
    std::string json_object, Options opts) {
  return std::make_shared<internal::ServiceAccountConfig>(
      std::move(json_object), std::move(opts));
}

std::shared_ptr<Credentials> MakeExternalAccountCredentials(
    std::string json_object, Options opts) {
  return std::make_shared<internal::ExternalAccountConfig>(
      std::move(json_object), std::move(opts));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
