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
#include "google/cloud/internal/parse_service_account_p12_file.h"
#include <fstream>

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

StatusOr<std::shared_ptr<oauth2_internal::Credentials>>
CreateServiceAccountCredentialsFromJsonContents(
    std::string const& contents, Options const& options,
    oauth2_internal::HttpClientFactory client_factory) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto info =
      oauth2_internal::ParseServiceAccountCredentials(contents, "memory");
  if (!info) {
    std::cout << __PRETTY_FUNCTION__ << ": parse error" << std::endl;
    return info.status();
  }
  std::cout << __PRETTY_FUNCTION__ << ": info->client_email='"
            << info->client_email << "'" << std::endl;

  // Verify this is usable before returning it.
  auto const tp = std::chrono::system_clock::time_point{};
  auto const components = AssertionComponentsFromInfo(*info, tp);
  auto jwt = internal::MakeJWTAssertionNoThrow(
      components.first, components.second, info->private_key);
  // if (!jwt) return MakeErrorCredentials(std::move(jwt).status());
  if (!jwt) return jwt.status();
  return StatusOr<std::shared_ptr<oauth2_internal::Credentials>>(
      std::make_shared<oauth2_internal::ServiceAccountCredentials>(
          *info, options, std::move(client_factory)));
}

StatusOr<std::shared_ptr<oauth2_internal::Credentials>>
CreateServiceAccountCredentialsFromJsonFilePath(
    std::string const& path, absl::optional<std::set<std::string>>,
    absl::optional<std::string>, Options const& options,
    oauth2_internal::HttpClientFactory client_factory) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  std::ifstream is(path);
  std::string contents(std::istreambuf_iterator<char>{is}, {});
  return CreateServiceAccountCredentialsFromJsonContents(
      std::move(contents), options, std::move(client_factory));
#if 0
  auto info = ParseServiceAccountCredentials(contents, path);
  if (!info) {
    return StatusOr<std::shared_ptr<Credentials>>(info.status());
  }
  // These are supplied as extra parameters to this method, not in the JSON
  // file.
  info->subject = std::move(subject);
  info->scopes = std::move(scopes);
  return StatusOr<std::shared_ptr<Credentials>>(
      std::make_shared<ServiceAccountCredentials<>>(*info, options));
#endif
}

std::shared_ptr<oauth2_internal::Credentials>
CreateServiceAccountCredentialsFromP12FilePath(
    std::string const& path, absl::optional<std::set<std::string>> scopes,
    absl::optional<std::string> subject, Options const& options,
    oauth2_internal::HttpClientFactory client_factory) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto info = oauth2_internal::ParseServiceAccountP12File(path);
  if (!info) {
    std::cout << __PRETTY_FUNCTION__ << ": return ErrorCredentials"
              << std::endl;
    return MakeErrorCredentials(std::move(info).status());
    // return StatusOr<std::shared_ptr<Credentials>>(info.status());
  }
  std::cout << __PRETTY_FUNCTION__ << ": info->client_email='"
            << info->client_email << "'" << std::endl;

  // These are supplied as extra parameters to this method, not in the P12
  // file.
  info->subject = std::move(subject);
  info->scopes = std::move(scopes);
  return std::make_shared<oauth2_internal::ServiceAccountCredentials>(
      *info, options, std::move(client_factory));
}

std::shared_ptr<oauth2_internal::Credentials>
CreateServiceAccountCredentialsFromFilePath(
    std::string const& path, absl::optional<std::set<std::string>> scopes,
    absl::optional<std::string> subject, Options const& options,
    oauth2_internal::HttpClientFactory client_factory) {
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  auto credentials = CreateServiceAccountCredentialsFromJsonFilePath(
      path, scopes, subject, options, client_factory);
  if (credentials) {
    std::cout << __PRETTY_FUNCTION__ << ": return JSON credentials"
              << std::endl;
    return *credentials;
  }
  return CreateServiceAccountCredentialsFromP12FilePath(
      path, std::move(scopes), std::move(subject), options,
      std::move(client_factory));
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
      if (cfg.file_path().has_value()) {
        result = Decorate(CreateServiceAccountCredentialsFromFilePath(
                              *cfg.file_path(), {}, {}, cfg.options(),
                              std::move(client_factory_)),
                          cfg.options());
      } else {
        auto creds = CreateServiceAccountCredentialsFromJsonContents(
            cfg.json_object(), cfg.options(), std::move(client_factory_));
        if (creds) {
          result = Decorate(std::move(*creds), cfg.options());
          return;
        }
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
