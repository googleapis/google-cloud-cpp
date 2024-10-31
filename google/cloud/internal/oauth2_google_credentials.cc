// Copyright 2018 Google LLC
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

#include "google/cloud/internal/oauth2_google_credentials.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/make_jwt_assertion.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/oauth2_authorized_user_credentials.h"
#include "google/cloud/internal/oauth2_compute_engine_credentials.h"
#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/internal/oauth2_external_account_credentials.h"
#include "google/cloud/internal/oauth2_google_application_default_credentials_file.h"
#include "google/cloud/internal/oauth2_http_client_factory.h"
#include "google/cloud/internal/oauth2_impersonate_service_account_credentials.h"
#include "google/cloud/internal/oauth2_service_account_credentials.h"
#include "google/cloud/internal/parse_service_account_p12_file.h"
#include "google/cloud/internal/throw_delegate.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iterator>
#include <memory>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// NOLINTNEXTLINE(misc-no-recursion)
StatusOr<std::unique_ptr<Credentials>> LoadCredsFromString(
    std::string const& contents, std::string const& path,
    Options const& options, HttpClientFactory client_factory) {
  auto const cred_json = nlohmann::json::parse(contents, nullptr, false);
  auto const cred_type = cred_json.value("type", "no type given");
  // If non_service_account_ok==false and the cred_type is authorized_user,
  // we'll return "Unsupported credential type (authorized_user)".
  if (cred_type == "authorized_user") {
    auto info = ParseAuthorizedUserCredentials(contents, path);
    if (!info) return std::move(info).status();
    return std::unique_ptr<Credentials>(
        std::make_unique<AuthorizedUserCredentials>(*info, options,
                                                    std::move(client_factory)));
  }
  if (cred_type == "external_account") {
    auto info =
        ParseExternalAccountConfiguration(contents, internal::ErrorContext{});
    if (!info) return std::move(info).status();
    return std::unique_ptr<Credentials>(
        std::make_unique<ExternalAccountCredentials>(
            *std::move(info), std::move(client_factory), options));
  }
  if (cred_type == "service_account") {
    auto info = ParseServiceAccountCredentials(contents, path);
    if (!info) return std::move(info).status();
    return std::unique_ptr<Credentials>(
        std::make_unique<ServiceAccountCredentials>(*info, options,
                                                    std::move(client_factory)));
  }
  if (cred_type == "impersonated_service_account") {
    auto info = ParseImpersonatedServiceAccountCredentials(contents, path);
    if (!info) return std::move(info).status();
    auto source_creds = LoadCredsFromString(std::move(info->source_credentials),
                                            path, options, client_factory);
    if (!source_creds) return std::move(source_creds).status();

    auto opts = options;
    auto& delegates = opts.lookup<DelegatesOption>();
    for (auto& delegate : info->delegates) {
      delegates.push_back(std::move(delegate));
    }

    internal::ImpersonateServiceAccountConfig config(
        // The base credentials (GUAC) are used to create the IAM REST Stub. We
        // are going to override them by supplying our own IAM REST Stub,
        // constructed using `oauth2_internal::Credentials`.
        nullptr, std::move(info->service_account), opts);
    auto rest_stub = MakeMinimalIamCredentialsRestStub(
        *std::move(source_creds), opts, std::move(client_factory));
    return std::unique_ptr<Credentials>(
        std::make_unique<ImpersonateServiceAccountCredentials>(
            config, std::move(rest_stub)));
  }
  return internal::InvalidArgumentError(
      "Unsupported credential type (" + cred_type +
          ") when reading Application Default Credentials file "
          "from " +
          path + ".",
      GCP_ERROR_INFO());
}

// Parses the JSON at `path` and creates the appropriate
// Credentials type.
//
// If `service_account_scopes` or `service_account_subject` are specified, the
// file at `path` must be a JSON service account. If a different type of
// credential file is found, this function returns nullptr to indicate a service
// account file wasn't found.
StatusOr<std::unique_ptr<Credentials>> LoadCredsFromPath(
    std::string const& path, Options const& options,
    HttpClientFactory client_factory) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    // We use kUnknown here because we don't know if the file does not exist, or
    // if we were unable to open it for some other reason.
    return internal::UnknownError("Cannot open credentials file " + path,
                                  GCP_ERROR_INFO());
  }
  auto const contents = std::string(std::istreambuf_iterator<char>{ifs}, {});
  auto cred_json = nlohmann::json::parse(contents, nullptr, false);
  if (!cred_json.is_object()) {
    // This is not a JSON file, try to load it as a P12 service account.
    auto info = ParseServiceAccountP12File(path);
    if (!info) {
      return internal::InvalidArgumentError(
          "Cannot open credentials file " + path +
              ", it does not contain a JSON object, nor can be parsed "
              "as a PKCS#12 file. " +
              info.status().message(),
          GCP_ERROR_INFO());
    }
    info->scopes = {};
    info->subject = {};
    return std::unique_ptr<Credentials>(
        std::make_unique<ServiceAccountCredentials>(*info, options,
                                                    std::move(client_factory)));
  }
  return LoadCredsFromString(contents, path, options,
                             std::move(client_factory));
}

// Tries to load the file at the path specified by the value of the Application
// Default %Credentials environment variable and to create the appropriate
// Credentials type.
//
// Returns nullptr if the environment variable is not set or the path does not
// exist.
//
// If `service_account_scopes` or `service_account_subject` are specified, the
// found file must be a JSON service account. If a different type of credential
// file is found, this function returns nullptr to indicate a service account
// file wasn't found.
StatusOr<std::unique_ptr<Credentials>> MaybeLoadCredsFromAdcPaths(
    Options const& options, HttpClientFactory client_factory) {
  // 1) Check if the GOOGLE_APPLICATION_CREDENTIALS environment variable is set.
  auto path = GoogleAdcFilePathFromEnvVarOrEmpty();
  if (path.empty()) {
    // 2) If no path was specified via environment variable, check if the
    // gcloud ADC file exists.
    path = GoogleAdcFilePathFromWellKnownPathOrEmpty();
    if (path.empty()) return {nullptr};
    // Just because we had the necessary information to build the path doesn't
    // mean that a file exists there.
    std::error_code ec;
    auto adc_file_status = google::cloud::internal::status(path, ec);
    if (!google::cloud::internal::exists(adc_file_status)) return {nullptr};
  }

  // If the path was specified, try to load that file; explicitly fail if it
  // doesn't exist or can't be read and parsed.
  return LoadCredsFromPath(path, options, std::move(client_factory));
}

}  // namespace

StatusOr<std::shared_ptr<Credentials>> GoogleDefaultCredentials(
    Options const& options, HttpClientFactory client_factory) {
  // 1 and 2) Check if the GOOGLE_APPLICATION_CREDENTIALS environment variable
  // is set or if the gcloud ADC file exists.
  auto creds = MaybeLoadCredsFromAdcPaths(options, client_factory);
  if (!creds) return std::move(creds).status();
  if (*creds) return std::shared_ptr<Credentials>(*std::move(creds));

  // 3) Check for implicit environment-based credentials (GCE, GAE Flexible,
  // Cloud Run or GKE Environment).
  return std::shared_ptr<Credentials>(
      std::make_shared<ComputeEngineCredentials>(options,
                                                 std::move(client_factory)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
