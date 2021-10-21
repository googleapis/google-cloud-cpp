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

#include "google/cloud/storage/internal/unified_rest_credentials.h"
#include "google/cloud/storage/internal/access_token_credentials.h"
#include "google/cloud/storage/internal/error_credentials.h"
#include "google/cloud/storage/internal/impersonate_service_account_credentials.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

using ::google::cloud::internal::AccessTokenConfig;
using ::google::cloud::internal::CredentialsVisitor;
using ::google::cloud::internal::GoogleDefaultCredentialsConfig;
using ::google::cloud::internal::ImpersonateServiceAccountConfig;
using ::google::cloud::internal::InsecureCredentialsConfig;
using ::google::cloud::internal::ServiceAccountConfig;

std::shared_ptr<oauth2::Credentials> MapCredentials(
    std::shared_ptr<google::cloud::Credentials> const& credentials) {
  struct Visitor : public CredentialsVisitor {
    std::shared_ptr<oauth2::Credentials> result;

    void visit(InsecureCredentialsConfig&) override {
      result = google::cloud::storage::oauth2::CreateAnonymousCredentials();
    }
    void visit(GoogleDefaultCredentialsConfig&) override {
      auto credentials =
          google::cloud::storage::oauth2::GoogleDefaultCredentials();
      if (credentials) {
        result = *std::move(credentials);
        return;
      }
      result =
          std::make_shared<ErrorCredentials>(std::move(credentials).status());
    }
    void visit(AccessTokenConfig& config) override {
      result = std::make_shared<AccessTokenCredentials>(config.access_token());
    }
    void visit(ImpersonateServiceAccountConfig& config) override {
      result = std::make_shared<ImpersonateServiceAccountCredentials>(config);
    }
    void visit(ServiceAccountConfig& cfg) override {
      auto credentials = google::cloud::storage::oauth2::
          CreateServiceAccountCredentialsFromJsonContents(cfg.json_object());
      if (credentials) {
        result = *std::move(credentials);
        return;
      }
      result =
          std::make_shared<ErrorCredentials>(std::move(credentials).status());
    }
  } visitor;

  CredentialsVisitor::dispatch(*credentials, visitor);
  return std::move(visitor.result);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
