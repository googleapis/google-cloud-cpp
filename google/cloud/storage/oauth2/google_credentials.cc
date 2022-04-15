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

#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/internal/make_jwt_assertion.h"
#include "google/cloud/storage/oauth2/anonymous_credentials.h"
#include "google/cloud/storage/oauth2/authorized_user_credentials.h"
#include "google/cloud/storage/oauth2/compute_engine_credentials.h"
#include "google/cloud/storage/oauth2/google_application_default_credentials_file.h"
#include "google/cloud/storage/oauth2/service_account_credentials.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/throw_delegate.h"
#include "absl/memory/memory.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iterator>
#include <memory>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace oauth2 {

char constexpr kAdcLink[] =
    "https://developers.google.com/identity/protocols/"
    "application-default-credentials";

// Parses the JSON or P12 file at `path` and creates the appropriate
// Credentials type.
//
// If `service_account_scopes` or `service_account_subject` are specified, the
// file at `path` must be a P12 service account or a JSON service account. If
// a different type of credential file is found, this function returns
// nullptr to indicate a service account file wasn't found.
StatusOr<std::unique_ptr<Credentials>> LoadCredsFromPath(
    std::string const& path, bool non_service_account_ok,
    absl::optional<std::set<std::string>> service_account_scopes,
    absl::optional<std::string> service_account_subject,
    ChannelOptions const& options) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    // We use kUnknown here because we don't know if the file does not exist, or
    // if we were unable to open it for some other reason.
    return Status(StatusCode::kUnknown, "Cannot open credentials file " + path);
  }
  std::string contents(std::istreambuf_iterator<char>{ifs}, {});
  auto cred_json = nlohmann::json::parse(contents, nullptr, false);
  if (!cred_json.is_object()) {
    // This is not a JSON file, try to load it as a P12 service account.
    auto info = ParseServiceAccountP12File(path);
    if (!info) {
      return Status(
          StatusCode::kInvalidArgument,
          "Cannot open credentials file " + path +
              ", it does not contain a JSON object, nor can be parsed "
              "as a PKCS#12 file. " +
              info.status().message());
    }
    info->subject = std::move(service_account_subject);
    info->scopes = std::move(service_account_scopes);
    auto credentials = absl::make_unique<ServiceAccountCredentials<>>(*info);
    return std::unique_ptr<Credentials>(std::move(credentials));
  }
  std::string cred_type = cred_json.value("type", "no type given");
  // If non_service_account_ok==false and the cred_type is authorized_user,
  // we'll return "Unsupported credential type (authorized_user)".
  if (cred_type == "authorized_user" && non_service_account_ok) {
    if (service_account_scopes || service_account_subject) {
      // No ptr indicates that the file we found was not a service account file.
      return StatusOr<std::unique_ptr<Credentials>>(nullptr);
    }
    auto info = ParseAuthorizedUserCredentials(contents, path);
    if (!info) {
      return info.status();
    }
    std::unique_ptr<Credentials> ptr =
        absl::make_unique<AuthorizedUserCredentials<>>(*info);
    return StatusOr<std::unique_ptr<Credentials>>(std::move(ptr));
  }
  if (cred_type == "service_account") {
    auto info = ParseServiceAccountCredentials(contents, path);
    if (!info) {
      return info.status();
    }
    info->subject = std::move(service_account_subject);
    info->scopes = std::move(service_account_scopes);
    std::unique_ptr<Credentials> ptr =
        absl::make_unique<ServiceAccountCredentials<>>(*info, options);
    return StatusOr<std::unique_ptr<Credentials>>(std::move(ptr));
  }
  return StatusOr<std::unique_ptr<Credentials>>(
      Status(StatusCode::kInvalidArgument,
             "Unsupported credential type (" + cred_type +
                 ") when reading Application Default Credentials file from " +
                 path + "."));
}

// Tries to load the file at the path specified by the value of the Application
// Default %Credentials environment variable and to create the appropriate
// Credentials type.
//
// Returns nullptr if the environment variable is not set or the path does not
// exist.
//
// If `service_account_scopes` or `service_account_subject` are specified, the
// found file must be a P12 service account or a JSON service account. If a
// different type of credential file is found, this function returns nullptr
// to indicate a service account file wasn't found.
StatusOr<std::unique_ptr<Credentials>> MaybeLoadCredsFromAdcPaths(
    bool non_service_account_ok,
    absl::optional<std::set<std::string>> service_account_scopes,
    absl::optional<std::string> service_account_subject,
    ChannelOptions const& options = {}) {
  // 1) Check if the GOOGLE_APPLICATION_CREDENTIALS environment variable is set.
  auto path = GoogleAdcFilePathFromEnvVarOrEmpty();
  if (path.empty()) {
    // 2) If no path was specified via environment variable, check if the
    // gcloud ADC file exists.
    path = GoogleAdcFilePathFromWellKnownPathOrEmpty();
    if (path.empty()) {
      return StatusOr<std::unique_ptr<Credentials>>(nullptr);
    }
    // Just because we had the necessary information to build the path doesn't
    // mean that a file exists there.
    std::error_code ec;
    auto adc_file_status = google::cloud::internal::status(path, ec);
    if (!google::cloud::internal::exists(adc_file_status)) {
      return StatusOr<std::unique_ptr<Credentials>>(nullptr);
    }
  }

  // If the path was specified, try to load that file; explicitly fail if it
  // doesn't exist or can't be read and parsed.
  return LoadCredsFromPath(path, non_service_account_ok,
                           std::move(service_account_scopes),
                           std::move(service_account_subject), options);
}

StatusOr<std::shared_ptr<Credentials>> GoogleDefaultCredentials(
    ChannelOptions const& options) {
  // 1 and 2) Check if the GOOGLE_APPLICATION_CREDENTIALS environment variable
  // is set or if the gcloud ADC file exists.
  auto creds = MaybeLoadCredsFromAdcPaths(true, {}, {}, options);
  if (!creds) {
    return StatusOr<std::shared_ptr<Credentials>>(creds.status());
  }
  if (*creds) {
    return StatusOr<std::shared_ptr<Credentials>>(std::move(*creds));
  }

  // 3) Check for implicit environment-based credentials (GCE, GAE Flexible,
  // Cloud Run or GKE Environment).
  auto gce_creds = std::make_shared<ComputeEngineCredentials<>>();
  auto override_val =
      google::cloud::internal::GetEnv(internal::GceCheckOverrideEnvVar());
  if (override_val.has_value() ? (std::string("1") == *override_val)
                               : gce_creds->AuthorizationHeader().ok()) {
    return StatusOr<std::shared_ptr<Credentials>>(std::move(gce_creds));
  }

  // We've exhausted all search points, thus credentials cannot be constructed.
  return StatusOr<std::shared_ptr<Credentials>>(
      Status(StatusCode::kUnknown,
             "Could not automatically determine credentials. For more "
             "information, please see " +
                 std::string(kAdcLink)));
}

std::shared_ptr<Credentials> CreateAnonymousCredentials() {
  return std::make_shared<AnonymousCredentials>();
}

StatusOr<std::shared_ptr<Credentials>>
CreateAuthorizedUserCredentialsFromJsonFilePath(std::string const& path) {
  std::ifstream is(path);
  std::string contents(std::istreambuf_iterator<char>{is}, {});
  auto info = ParseAuthorizedUserCredentials(contents, path);
  if (!info) {
    return StatusOr<std::shared_ptr<Credentials>>(info.status());
  }
  return StatusOr<std::shared_ptr<Credentials>>(
      std::make_shared<AuthorizedUserCredentials<>>(*info));
}

StatusOr<std::shared_ptr<Credentials>>
CreateAuthorizedUserCredentialsFromJsonContents(std::string const& contents,
                                                ChannelOptions const& options) {
  auto info = ParseAuthorizedUserCredentials(contents, "memory");
  if (!info) {
    return StatusOr<std::shared_ptr<Credentials>>(info.status());
  }
  return StatusOr<std::shared_ptr<Credentials>>(
      std::make_shared<AuthorizedUserCredentials<>>(*info, options));
}

StatusOr<std::shared_ptr<Credentials>>
CreateServiceAccountCredentialsFromFilePath(std::string const& path) {
  return CreateServiceAccountCredentialsFromFilePath(path, {}, {});
}

StatusOr<std::shared_ptr<Credentials>>
CreateServiceAccountCredentialsFromFilePath(
    std::string const& path, absl::optional<std::set<std::string>> scopes,
    absl::optional<std::string> subject) {
  auto credentials =
      CreateServiceAccountCredentialsFromJsonFilePath(path, scopes, subject);
  if (credentials) {
    return credentials;
  }
  return CreateServiceAccountCredentialsFromP12FilePath(path, std::move(scopes),
                                                        std::move(subject));
}

StatusOr<std::shared_ptr<Credentials>>
CreateServiceAccountCredentialsFromJsonFilePath(std::string const& path) {
  return CreateServiceAccountCredentialsFromJsonFilePath(path, {}, {});
}

StatusOr<std::shared_ptr<Credentials>>
CreateServiceAccountCredentialsFromJsonFilePath(
    std::string const& path, absl::optional<std::set<std::string>> scopes,
    absl::optional<std::string> subject, ChannelOptions const& options) {
  std::ifstream is(path);
  std::string contents(std::istreambuf_iterator<char>{is}, {});
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
}

StatusOr<std::shared_ptr<Credentials>>
CreateServiceAccountCredentialsFromP12FilePath(
    std::string const& path, absl::optional<std::set<std::string>> scopes,
    absl::optional<std::string> subject, ChannelOptions const& options) {
  auto info = ParseServiceAccountP12File(path);
  if (!info) {
    return StatusOr<std::shared_ptr<Credentials>>(info.status());
  }
  // These are supplied as extra parameters to this method, not in the P12
  // file.
  info->subject = std::move(subject);
  info->scopes = std::move(scopes);
  return StatusOr<std::shared_ptr<Credentials>>(
      std::make_shared<ServiceAccountCredentials<>>(*info, options));
}

StatusOr<std::shared_ptr<Credentials>>
CreateServiceAccountCredentialsFromP12FilePath(std::string const& path) {
  return CreateServiceAccountCredentialsFromP12FilePath(path, {}, {});
}

StatusOr<std::shared_ptr<Credentials>>
CreateServiceAccountCredentialsFromDefaultPaths(ChannelOptions const& options) {
  return CreateServiceAccountCredentialsFromDefaultPaths({}, {}, options);
}

StatusOr<std::shared_ptr<Credentials>>
CreateServiceAccountCredentialsFromDefaultPaths(
    absl::optional<std::set<std::string>> scopes,
    absl::optional<std::string> subject, ChannelOptions const& options) {
  auto creds = MaybeLoadCredsFromAdcPaths(false, std::move(scopes),
                                          std::move(subject), options);
  if (!creds) {
    return StatusOr<std::shared_ptr<Credentials>>(creds.status());
  }
  if (*creds) {
    return StatusOr<std::shared_ptr<Credentials>>(std::move(*creds));
  }

  // We've exhausted all search points, thus credentials cannot be constructed.
  return StatusOr<std::shared_ptr<Credentials>>(
      Status(StatusCode::kUnknown,
             "Could not create service account credentials using Application"
             "Default Credentials paths. For more information, please see " +
                 std::string(kAdcLink)));
}

StatusOr<std::shared_ptr<Credentials>>
CreateServiceAccountCredentialsFromJsonContents(std::string const& contents,
                                                ChannelOptions const& options) {
  return CreateServiceAccountCredentialsFromJsonContents(contents, {}, {},
                                                         options);
}

StatusOr<std::shared_ptr<Credentials>>
CreateServiceAccountCredentialsFromJsonContents(
    std::string const& contents, absl::optional<std::set<std::string>> scopes,
    absl::optional<std::string> subject, ChannelOptions const& options) {
  auto info = ParseServiceAccountCredentials(contents, "memory");
  if (!info) {
    return StatusOr<std::shared_ptr<Credentials>>(info.status());
  }
  std::chrono::system_clock::time_point now;
  auto components = AssertionComponentsFromInfo(*info, now);
  auto jwt_assertion = internal::MakeJWTAssertionNoThrow(
      components.first, components.second, info->private_key);
  if (!jwt_assertion) return std::move(jwt_assertion).status();

  // These are supplied as extra parameters to this method, not in the JSON
  // file.
  info->subject = std::move(subject);
  info->scopes = std::move(scopes);
  return StatusOr<std::shared_ptr<Credentials>>(
      std::make_shared<ServiceAccountCredentials<>>(*info, options));
}

std::shared_ptr<Credentials> CreateComputeEngineCredentials() {
  return std::make_shared<ComputeEngineCredentials<>>();
}

std::shared_ptr<Credentials> CreateComputeEngineCredentials(
    std::string const& service_account_email) {
  return std::make_shared<ComputeEngineCredentials<>>(service_account_email);
}

}  // namespace oauth2
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
