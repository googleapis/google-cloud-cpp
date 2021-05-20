// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_IMPERSONATE_SERVICE_ACCOUNT_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_IMPERSONATE_SERVICE_ACCOUNT_CREDENTIALS_H

#include "google/cloud/storage/internal/minimal_iam_credentials_rest.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/credentials.h"
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

class ImpersonateServiceAccountCredentials : public oauth2::Credentials {
 public:
  explicit ImpersonateServiceAccountCredentials(
      google::cloud::internal::ImpersonateServiceAccountConfig const& config);
  explicit ImpersonateServiceAccountCredentials(
      google::cloud::internal::ImpersonateServiceAccountConfig const& config,
      std::shared_ptr<MinimalIamCredentialsRest> stub);

  StatusOr<std::string> AuthorizationHeader() override;
  StatusOr<std::string> AuthorizationHeader(
      std::chrono::system_clock::time_point now);

 private:
  std::shared_ptr<MinimalIamCredentialsRest> stub_;
  GenerateAccessTokenRequest request_;
  std::mutex mu_;
  std::string header_;
  std::chrono::system_clock::time_point expiration_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_IMPERSONATE_SERVICE_ACCOUNT_CREDENTIALS_H
