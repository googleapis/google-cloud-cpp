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

#include "google/cloud/internal/unified_rest_credentials.h"
#include "google/cloud/internal/oauth2_access_token_credentials.h"
#include "google/cloud/internal/oauth2_error_credentials.h"
#include "google/cloud/internal/oauth2_google_credentials.h"
#include "google/cloud/internal/oauth2_impersonate_service_account_credentials.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::internal::AccessTokenConfig;
using ::google::cloud::internal::CredentialsVisitor;
using ::google::cloud::internal::GoogleDefaultCredentialsConfig;
using ::google::cloud::internal::ImpersonateServiceAccountConfig;
using ::google::cloud::internal::InsecureCredentialsConfig;
using ::google::cloud::internal::ServiceAccountConfig;

std::shared_ptr<oauth2_internal::Credentials> MapCredentials(
    std::shared_ptr<google::cloud::Credentials> const& credentials) {
  struct Visitor : public CredentialsVisitor {
    std::shared_ptr<oauth2_internal::Credentials> result;

    void visit(InsecureCredentialsConfig&) override {
      result = google::cloud::oauth2_internal::CreateAnonymousCredentials();
    }
    void visit(GoogleDefaultCredentialsConfig&) override {
      auto credentials =
          google::cloud::oauth2_internal::GoogleDefaultCredentials();
      if (credentials) {
        result = *std::move(credentials);
        return;
      }
      result = std::make_shared<oauth2_internal::ErrorCredentials>(
          std::move(credentials).status());
    }
    void visit(AccessTokenConfig& config) override {
      result = std::make_shared<oauth2_internal::AccessTokenCredentials>(
          config.access_token());
    }
    void visit(ImpersonateServiceAccountConfig& config) override {
      result = std::make_shared<
          oauth2_internal::ImpersonateServiceAccountCredentials>(config);
    }

    void visit(ServiceAccountConfig& cfg) override {
      auto credentials = google::cloud::oauth2_internal::
          CreateServiceAccountCredentialsFromJsonContents(cfg.json_object());
      if (credentials) {
        result = *std::move(credentials);
        return;
      }
      result = std::make_shared<oauth2_internal::ErrorCredentials>(
          std::move(credentials).status());
    }
  } visitor;

  CredentialsVisitor::dispatch(*credentials, visitor);
  return std::move(visitor.result);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
