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
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/make_jwt_assertion.h"
#include "google/cloud/internal/oauth2_authorized_user_credentials.h"
#include "google/cloud/internal/oauth2_compute_engine_credentials.h"
#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/internal/oauth2_google_application_default_credentials_file.h"
#include "google/cloud/internal/oauth2_service_account_credentials.h"
#include "google/cloud/internal/throw_delegate.h"
#include "absl/memory/memory.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iterator>
#include <memory>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

char constexpr kAdcLink[] =
    "https://developers.google.com/identity/protocols/"
    "application-default-credentials";

// Parses the JSON at `path` and creates the appropriate
// Credentials type.
//
// If `service_account_scopes` or `service_account_subject` are specified, the
// file at `path` must be a JSON service account. If a different type of
// credential file is found, this function returns nullptr to indicate a service
// account file wasn't found.
StatusOr<std::unique_ptr<Credentials>> LoadCredsFromPath(
    std::string const& path, bool non_service_account_ok,
    absl::optional<std::set<std::string>> service_account_scopes,
    absl::optional<std::string> service_account_subject,
    Options const& options) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    // We use kUnknown here because we don't know if the file does not exist, or
    // if we were unable to open it for some other reason.
    return Status(StatusCode::kUnknown, "Cannot open credentials file " + path);
  }
  std::string contents(std::istreambuf_iterator<char>{ifs}, {});
  auto cred_json = nlohmann::json::parse(contents, nullptr, false);
  if (!cred_json.is_object()) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid credentials file " + path);
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
        absl::make_unique<AuthorizedUserCredentials>(*info);
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
        absl::make_unique<ServiceAccountCredentials>(*info, options);
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
// found file must be a JSON service account. If a different type of credential
// file is found, this function returns nullptr to indicate a service account
// file wasn't found.
StatusOr<std::unique_ptr<Credentials>> MaybeLoadCredsFromAdcPaths(
    bool non_service_account_ok,
    absl::optional<std::set<std::string>> service_account_scopes,
    absl::optional<std::string> service_account_subject,
    Options const& options = {}) {
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

}  // namespace

StatusOr<std::shared_ptr<Credentials>> GoogleDefaultCredentials(
    Options const& options) {
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
  auto gce_creds = std::make_shared<ComputeEngineCredentials>();
  if (options.get<oauth2_internal::ForceGceOption>() ||
      gce_creds->AuthorizationHeader().ok()) {
    return std::shared_ptr<Credentials>(std::move(gce_creds));
  }

  // We've exhausted all search points, thus credentials cannot be constructed.
  return StatusOr<std::shared_ptr<Credentials>>(
      Status(StatusCode::kUnknown,
             "Could not automatically determine credentials. For more "
             "information, please see " +
                 std::string(kAdcLink)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
