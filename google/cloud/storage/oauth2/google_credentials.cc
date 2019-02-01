// Copyright 2018 Google LLC
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

#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/oauth2/anonymous_credentials.h"
#include "google/cloud/storage/oauth2/authorized_user_credentials.h"
#include "google/cloud/storage/oauth2/compute_engine_credentials.h"
#include "google/cloud/storage/oauth2/google_application_default_credentials_file.h"
#include "google/cloud/storage/oauth2/service_account_credentials.h"
#include <fstream>
#include <iterator>
#include <memory>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {

/// Parses the JSON file at `path` and creates the appropriate Credentials type.
StatusOr<std::unique_ptr<Credentials>> LoadCredsFromPath(
    std::string const& path) {
  namespace nl = google::cloud::storage::internal::nl;

  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    // We use kUnknown here because we don't know if the file does not exist, or
    // if we were unable to open it for some other reason.
    return Status(StatusCode::kUnknown, "Cannot open credentials file " + path);
  }
  std::string contents(std::istreambuf_iterator<char>{ifs}, {});
  auto cred_json = nl::json::parse(contents, nullptr, false);
  if (cred_json.is_discarded()) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid contents in credentials file " + path);
  }
  std::string cred_type = cred_json.value("type", "no type given");
  if (cred_type == "authorized_user") {
    auto info = ParseAuthorizedUserCredentials(contents, path);
    if (!info) {
      return info.status();
    }
    std::unique_ptr<Credentials> ptr =
        google::cloud::internal::make_unique<AuthorizedUserCredentials<>>(
            *info);
    return StatusOr<std::unique_ptr<Credentials>>(std::move(ptr));
  }
  if (cred_type == "service_account") {
    auto info = ParseServiceAccountCredentials(contents, path);
    if (!info) {
      return info.status();
    }
    std::unique_ptr<Credentials> ptr =
        google::cloud::internal::make_unique<ServiceAccountCredentials<>>(
            *info);
    return StatusOr<std::unique_ptr<Credentials>>(std::move(ptr));
  }
  return StatusOr<std::unique_ptr<Credentials>>(
      Status(StatusCode::kInvalidArgument,
             "Unsupported credential type (" + cred_type +
                 ") when reading Application Default Credentials file from " +
                 path + "."));
}

StatusOr<std::unique_ptr<Credentials>> MaybeLoadCredsFromAdcEnvVar() {
  auto path = GoogleAdcFilePathFromEnvVarOrEmpty();
  if (!path.empty()) {
    // If the path was specified, try to load that file; explicitly fail if it
    // doesn't exist or can't be read and parsed.
    return LoadCredsFromPath(path);
  }
  // No ptr indicates that there was no path to attempt loading creds from.
  return StatusOr<std::unique_ptr<Credentials>>(nullptr);
}

StatusOr<std::unique_ptr<Credentials>> MaybeLoadCredsFromGcloudAdcFile() {
  auto path = GoogleAdcFilePathFromWellKnownPathOrEmpty();
  if (!path.empty()) {
    // Just because we had the necessary information to build the path doesn't
    // mean that a file exists there.
    std::error_code ec;
    auto adc_file_status = google::cloud::internal::status(path, ec);
    if (google::cloud::internal::exists(adc_file_status)) {
      return LoadCredsFromPath(path);
    }
  }
  // Either we were unable to construct the well known path or no file existed
  // at that path.
  return StatusOr<std::unique_ptr<Credentials>>(nullptr);
}

StatusOr<std::shared_ptr<Credentials>> GoogleDefaultCredentials() {
  // 1) Check if the GOOGLE_APPLICATION_CREDENTIALS environment variable is set.
  auto creds = MaybeLoadCredsFromAdcEnvVar();
  if (!creds) {
    return StatusOr<std::shared_ptr<Credentials>>(creds.status());
  }
  if (*creds) {
    return StatusOr<std::shared_ptr<Credentials>>(std::move(*creds));
  }

  // 2) If no path was specified via environment variable, check if the gcloud
  // ADC file exists.
  creds = MaybeLoadCredsFromGcloudAdcFile();
  if (!creds) {
    return StatusOr<std::shared_ptr<Credentials>>(creds.status());
  }
  if (*creds) {
    return StatusOr<std::shared_ptr<Credentials>>(std::move(*creds));
  }

  // 3) Check for implicit environment-based credentials (GCE, GAE Flexible
  // Environment).
  // Note: GCE credentials *should* also work when running on a VM instance in
  // the App Engine Flexible Environment, but this has not been explicitly
  // tested, as it requires a custom GAEF runtime.
  if (storage::internal::RunningOnComputeEngineVm()) {
    return StatusOr<std::shared_ptr<Credentials>>(
        std::make_shared<ComputeEngineCredentials<>>());
  }

  // We've exhausted all search points, thus credentials cannot be constructed.
  std::string adc_link =
      "https://developers.google.com/identity/protocols"
      "/application-default-credentials";
  return StatusOr<std::shared_ptr<Credentials>>(
      Status(StatusCode::kUnknown,
             "Could not automatically determine credentials. For more "
             "information, please see " +
                 adc_link));
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
CreateAuthorizedUserCredentialsFromJsonContents(std::string const& contents) {
  auto info = ParseAuthorizedUserCredentials(contents, "memory");
  if (!info) {
    return StatusOr<std::shared_ptr<Credentials>>(info.status());
  }
  return StatusOr<std::shared_ptr<Credentials>>(
      std::make_shared<AuthorizedUserCredentials<>>(*info));
}

StatusOr<std::shared_ptr<Credentials>>
CreateServiceAccountCredentialsFromJsonFilePath(std::string const& path) {
  std::ifstream is(path);
  std::string contents(std::istreambuf_iterator<char>{is}, {});
  auto info = ParseServiceAccountCredentials(contents, path);
  if (!info) {
    return StatusOr<std::shared_ptr<Credentials>>(info.status());
  }
  return StatusOr<std::shared_ptr<Credentials>>(
      std::make_shared<ServiceAccountCredentials<>>(*info));
}

StatusOr<std::shared_ptr<Credentials>>
CreateServiceAccountCredentialsFromJsonContents(std::string const& contents) {
  auto info = ParseServiceAccountCredentials(contents, "memory");
  if (!info) {
    return StatusOr<std::shared_ptr<Credentials>>(info.status());
  }
  return StatusOr<std::shared_ptr<Credentials>>(
      std::make_shared<ServiceAccountCredentials<>>(*info));
}

std::shared_ptr<Credentials> CreateComputeEngineCredentials() {
  return std::make_shared<ComputeEngineCredentials<>>();
}

std::shared_ptr<Credentials> CreateComputeEngineCredentials(
    std::string const& service_account_email) {
  return std::make_shared<ComputeEngineCredentials<>>(service_account_email);
}

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
