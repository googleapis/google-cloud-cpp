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

#include "google/cloud/internal/credentials_impl.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/populate_common_options.h"
#include <chrono>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

auto constexpr kDefaultScope = "https://www.googleapis.com/auth/cloud-platform";
auto constexpr kDefaultTokenLifetime = std::chrono::hours(1);

}  // namespace

Options PopulateAuthOptions(Options options) {
  // First set any defaults that may be missing.
  options =
      MergeOptions(std::move(options),
                   Options{}
                       .set<ScopesOption>({kDefaultScope})
                       .set<AccessTokenLifetimeOption>(kDefaultTokenLifetime));
  // Then apply any overrides.
  return MergeOptions(
      Options{}.set<TracingComponentsOption>(DefaultTracingComponents()),
      std::move(options));
}

void CredentialsVisitor::dispatch(Credentials& credentials,
                                  CredentialsVisitor& visitor) {
  credentials.dispatch(visitor);
}

InsecureCredentialsConfig::InsecureCredentialsConfig(Options opts)
    : options_(PopulateAuthOptions(std::move(opts))) {}

GoogleDefaultCredentialsConfig::GoogleDefaultCredentialsConfig(Options opts)
    : options_(PopulateAuthOptions(std::move(opts))) {}

AccessTokenConfig::AccessTokenConfig(
    std::string token, std::chrono::system_clock::time_point expiration,
    Options opts)
    : access_token_(AccessToken{std::move(token), expiration}),
      options_(PopulateAuthOptions(std::move(opts))) {}

ImpersonateServiceAccountConfig::ImpersonateServiceAccountConfig(
    std::shared_ptr<Credentials> base_credentials,
    std::string target_service_account, Options opts)
    : base_credentials_(std::move(base_credentials)),
      target_service_account_(std::move(target_service_account)),
      options_(PopulateAuthOptions(std::move(opts))) {}

std::chrono::seconds ImpersonateServiceAccountConfig::lifetime() const {
  return options_.get<AccessTokenLifetimeOption>();
}

std::vector<std::string> const& ImpersonateServiceAccountConfig::scopes()
    const {
  return options_.get<ScopesOption>();
}

std::vector<std::string> const& ImpersonateServiceAccountConfig::delegates()
    const {
  return options_.get<DelegatesOption>();
}

ServiceAccountConfig::ServiceAccountConfig(std::string json_object,
                                           Options opts)
    : json_object_(std::move(json_object)),
      options_(PopulateAuthOptions(std::move(opts))) {}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
