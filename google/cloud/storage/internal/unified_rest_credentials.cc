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

#include "google/cloud/storage/internal/unified_rest_credentials.h"
#include "google/cloud/storage/internal/access_token_credentials.h"
#include "google/cloud/storage/internal/error_credentials.h"
#include "google/cloud/storage/internal/impersonate_service_account_credentials.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/oauth2/service_account_credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/internal/oauth2_google_credentials.h"
#include "google/cloud/internal/oauth2_service_account_credentials.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::internal::AccessTokenConfig;
using ::google::cloud::internal::CredentialsVisitor;
using ::google::cloud::internal::GoogleDefaultCredentialsConfig;
using ::google::cloud::internal::ImpersonateServiceAccountConfig;
using ::google::cloud::internal::InsecureCredentialsConfig;
using ::google::cloud::internal::ServiceAccountConfig;

std::shared_ptr<oauth2::Credentials> MakeErrorCredentials(Status status) {
  return std::make_shared<ErrorCredentials>(std::move(status));
}

class WrapRestCredentials : public oauth2::Credentials {
 public:
  explicit WrapRestCredentials(
      std::shared_ptr<oauth2_internal::Credentials> impl)
      : impl_(std::move(impl)) {}

  StatusOr<std::string> AuthorizationHeader() override {
    return oauth2_internal::AuthorizationHeaderJoined(*impl_);
  }

  StatusOr<std::vector<std::uint8_t>> SignBlob(
      SigningAccount const& signing_account,
      std::string const& blob) const override {
    return impl_->SignBlob(signing_account.value_or(impl_->AccountEmail()),
                           blob);
  }

  std::string AccountEmail() const override { return impl_->AccountEmail(); }
  std::string KeyId() const override { return impl_->KeyId(); }

 private:
  std::shared_ptr<oauth2_internal::Credentials> impl_;
};

struct RestVisitor : public CredentialsVisitor {
  std::shared_ptr<oauth2::Credentials> result;

  void visit(InsecureCredentialsConfig&) override {
    result = google::cloud::storage::oauth2::CreateAnonymousCredentials();
  }
  void visit(GoogleDefaultCredentialsConfig&) override {
    auto credentials = oauth2_internal::GoogleDefaultCredentials();
    if (credentials) {
      result = std::make_shared<WrapRestCredentials>(*std::move(credentials));
      return;
    }
    result = MakeErrorCredentials(std::move(credentials).status());
  }
  void visit(AccessTokenConfig& config) override {
    result = std::make_shared<AccessTokenCredentials>(config.access_token());
  }
  void visit(ImpersonateServiceAccountConfig& config) override {
    result = std::make_shared<ImpersonateServiceAccountCredentials>(config);
  }
  void visit(ServiceAccountConfig& cfg) override {
    auto info = oauth2::ParseServiceAccountCredentials(cfg.json_object(), {});
    if (!info) {
      result = MakeErrorCredentials(std::move(info).status());
      return;
    }
    result = std::make_shared<WrapRestCredentials>(
        std::make_shared<oauth2_internal::ServiceAccountCredentials>(
            internal::MapServiceAccountCredentialsInfo(*std::move(info))));
  }
};

}  // namespace

std::shared_ptr<oauth2::Credentials> MapCredentials(
    std::shared_ptr<google::cloud::Credentials> const& credentials) {
  RestVisitor visitor;
  CredentialsVisitor::dispatch(*credentials, visitor);
  return std::move(visitor.result);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
