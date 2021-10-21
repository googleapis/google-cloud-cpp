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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ACCESS_TOKEN_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ACCESS_TOKEN_CREDENTIALS_H

#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/credentials_impl.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

class AccessTokenCredentials : public oauth2::Credentials {
 public:
  explicit AccessTokenCredentials(
      google::cloud::internal::AccessToken const& access_token);

  StatusOr<std::string> AuthorizationHeader() override;

 private:
  std::string header_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ACCESS_TOKEN_CREDENTIALS_H
