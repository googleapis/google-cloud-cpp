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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_MINIMAL_IAM_CREDENTIALS_REST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_MINIMAL_IAM_CREDENTIALS_REST_H

#include "google/cloud//version.h"
#include "google/cloud/internal/credentials_impl.h"
#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include <chrono>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct GenerateAccessTokenRequest {
  std::string service_account;
  std::chrono::seconds lifetime;
  std::vector<std::string> scopes;
  std::vector<std::string> delegates;
};

class MinimalIamCredentialsRest {
 public:
  virtual ~MinimalIamCredentialsRest() = default;

  virtual StatusOr<google::cloud::internal::AccessToken> GenerateAccessToken(
      GenerateAccessTokenRequest const& request) = 0;
};

std::shared_ptr<MinimalIamCredentialsRest> MakeMinimalIamCredentialsRestStub(
    std::shared_ptr<oauth2_internal::Credentials> credentials,
    Options options = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_MINIMAL_IAM_CREDENTIALS_REST_H
