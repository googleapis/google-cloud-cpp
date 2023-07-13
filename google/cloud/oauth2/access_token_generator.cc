// Copyright 2023 Google LLC
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

#include "google/cloud/oauth2/access_token_generator.h"
#include "google/cloud/internal/unified_rest_credentials.h"

namespace google {
namespace cloud {
namespace oauth2 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

class AccessTokenGeneratorImpl : public AccessTokenGenerator {
 public:
  explicit AccessTokenGeneratorImpl(
      std::shared_ptr<google::cloud::oauth2_internal::Credentials> creds)
      : creds_(std::move(creds)) {}
  ~AccessTokenGeneratorImpl() override = default;

  StatusOr<AccessToken> GetToken() override {
    return creds_->GetToken(std::chrono::system_clock::now());
  }

 private:
  std::shared_ptr<google::cloud::oauth2_internal::Credentials> creds_;
};

}  // namespace

std::shared_ptr<AccessTokenGenerator> MakeAccessTokenGenerator(
    Credentials const& credentials) {
  return std::make_shared<AccessTokenGeneratorImpl>(
      rest_internal::MapCredentials(credentials));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2
}  // namespace cloud
}  // namespace google
