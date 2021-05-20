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

#include "google/cloud/internal/credentials_impl.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

void CredentialsVisitor::dispatch(Credentials& credentials,
                                  CredentialsVisitor& visitor) {
  credentials.dispatch(visitor);
}

ImpersonateServiceAccountConfig::ImpersonateServiceAccountConfig(
    std::shared_ptr<Credentials> base_credentials,
    std::string target_service_account, Options opts)
    : base_credentials_(std::move(base_credentials)),
      target_service_account_(std::move(target_service_account)),
      lifetime_(opts.get<AccessTokenLifetimeOption>()),
      scopes_(std::move(opts.lookup<ScopesOption>())),
      delegates_(std::move(opts.lookup<DelegatesOption>())) {}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
