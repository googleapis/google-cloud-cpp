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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SELF_SIGNING_SERVICE_ACCOUNT_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SELF_SIGNING_SERVICE_ACCOUNT_CREDENTIALS_H

#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/version.h"
#include <chrono>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

struct SelfSigningServiceAccountCredentialsInfo {
  std::string client_email;
  std::string private_key_id;
  std::string private_key;
  std::string audience;
};

StatusOr<std::string> CreateBearerToken(
    SelfSigningServiceAccountCredentialsInfo const& info,
    std::chrono::system_clock::time_point tp);

class SelfSigningServiceAccountCredentials
    : public google::cloud::storage::oauth2::Credentials {
 public:
  explicit SelfSigningServiceAccountCredentials(
      SelfSigningServiceAccountCredentialsInfo info)
      : info_(std::move(info)) {}

  StatusOr<std::string> AuthorizationHeader() override;
  StatusOr<std::vector<std::uint8_t>> SignBlob(
      SigningAccount const& signing_account,
      std::string const& string_to_sign) const override;
  std::string AccountEmail() const override;
  std::string KeyId() const override;

 private:
  std::mutex mu_;
  SelfSigningServiceAccountCredentialsInfo info_;
  std::chrono::system_clock::time_point expiration_time_;
  std::string authorization_header_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SELF_SIGNING_SERVICE_ACCOUNT_CREDENTIALS_H
