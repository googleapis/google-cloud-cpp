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
#include "google/cloud/common_options.h"
#include "google/cloud/internal/make_jwt_assertion.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/oauth2_access_token_credentials.h"
#include "google/cloud/internal/oauth2_anonymous_credentials.h"
#include "google/cloud/internal/oauth2_api_key_credentials.h"
#include "google/cloud/internal/oauth2_compute_engine_credentials.h"
#include "google/cloud/internal/oauth2_decorate_credentials.h"
#include "google/cloud/internal/oauth2_error_credentials.h"
#include "google/cloud/internal/oauth2_external_account_credentials.h"
#include "google/cloud/internal/oauth2_google_credentials.h"
#include "google/cloud/internal/oauth2_impersonate_service_account_credentials.h"
#include "google/cloud/internal/oauth2_service_account_credentials.h"

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::AccessTokenConfig;
using ::google::cloud::internal::ApiKeyConfig;
using ::google::cloud::internal::ComputeEngineCredentialsConfig;
using ::google::cloud::internal::CredentialsVisitor;
using ::google::cloud::internal::ErrorCredentialsConfig;
using ::google::cloud::internal::ExternalAccountConfig;
using ::google::cloud::internal::GoogleDefaultCredentialsConfig;
using ::google::cloud::internal::ImpersonateServiceAccountConfig;
using ::google::cloud::internal::InsecureCredentialsConfig;
using ::google::cloud::internal::ServiceAccountConfig;
using ::google::cloud::oauth2_internal::Decorate;

std::shared_ptr<oauth2_internal::Credentials> MakeErrorCredentials(
    Status status) {
  return std::make_shared<oauth2_internal::ErrorCredentials>(std::move(status));
}

}  // namespace

std::shared_ptr<oauth2_internal::Credentials> MapCredentials(
    google::cloud::Credentials const& credentials) {
  return MapCredentials(credentials, [](Options const& options) {
    return MakeDefaultRestClient("", options);
  });
}

std::shared_ptr<oauth2_internal::Credentials> MapCredentials(
    google::cloud::Credentials const& credentials,
    oauth2_internal::HttpClientFactory client_factory) {
  class Visitor : public CredentialsVisitor {
   public:
    explicit Visitor(oauth2_internal::HttpClientFactory client_factory)
        : client_factory_(std::move(client_factory)) {}

    std::shared_ptr<oauth2_internal::Credentials> result;

    void visit(ErrorCredentialsConfig const& cfg) override {
      result =
          std::make_shared<oauth2_internal::ErrorCredentials>(cfg.status());
    }

    void visit(InsecureCredentialsConfig const&) override {
      result = std::make_shared<oauth2_internal::AnonymousCredentials>();
    }

    void visit(GoogleDefaultCredentialsConfig const& cfg) override {
      auto credentials =
          google::cloud::oauth2_internal::GoogleDefaultCredentials(
              cfg.options(), std::move(client_factory_));
      if (credentials) {
        result = Decorate(*std::move(credentials), cfg.options());
        return;
      }
      result = MakeErrorCredentials(std::move(credentials).status());
    }

    void visit(AccessTokenConfig const& cfg) override {
      result = std::make_shared<oauth2_internal::AccessTokenCredentials>(
          cfg.access_token());
    }

    void visit(ImpersonateServiceAccountConfig const& cfg) override {
      result = std::make_shared<
          oauth2_internal::ImpersonateServiceAccountCredentials>(
          cfg, std::move(client_factory_));
      result = Decorate(std::move(result), cfg.options());
    }

    void visit(ServiceAccountConfig const& cfg) override {
      StatusOr<std::shared_ptr<oauth2_internal::Credentials>> creds;
      if (cfg.file_path().has_value()) {
        creds = oauth2_internal::CreateServiceAccountCredentialsFromFilePath(
            *cfg.file_path(), cfg.options(), std::move(client_factory_));
      } else if (cfg.json_object().has_value()) {
        creds =
            oauth2_internal::CreateServiceAccountCredentialsFromJsonContents(
                *cfg.json_object(), cfg.options(), std::move(client_factory_));
      } else {
        creds = MakeErrorCredentials(internal::InternalError(
            "ServiceAccountConfig has neither json_object nor file_path",
            GCP_ERROR_INFO()));
      }

      if (creds.ok()) {
        result = Decorate(*creds, cfg.options());
      } else {
        result = MakeErrorCredentials(std::move(creds).status());
      }
    }

    void visit(ExternalAccountConfig const& cfg) override {
      auto const ec = internal::ErrorContext();
      auto info = oauth2_internal::ParseExternalAccountConfiguration(
          cfg.json_object(), ec);
      if (!info) {
        result = MakeErrorCredentials(std::move(info).status());
        return;
      }
      result = Decorate(
          std::make_shared<oauth2_internal::ExternalAccountCredentials>(
              *std::move(info), std::move(client_factory_), cfg.options()),
          cfg.options());
    }

    void visit(ApiKeyConfig const& cfg) override {
      result =
          std::make_shared<oauth2_internal::ApiKeyCredentials>(cfg.api_key());
    }

    void visit(ComputeEngineCredentialsConfig const& cfg) override {
      result = Decorate(
          std::make_shared<
              google::cloud::oauth2_internal::ComputeEngineCredentials>(
              cfg.options(), std::move(client_factory_)),
          cfg.options());
    }

   private:
    oauth2_internal::HttpClientFactory client_factory_;
  };

  Visitor visitor(std::move(client_factory));
  CredentialsVisitor::dispatch(credentials, visitor);
  return std::move(visitor.result);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
