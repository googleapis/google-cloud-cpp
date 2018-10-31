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

std::unique_ptr<Credentials> LoadCredsFromPath(std::string const& path) {
  using google::cloud::internal::RaiseRuntimeError;
  namespace nl = google::cloud::storage::internal::nl;

  std::ifstream ifs(path);
  if (not ifs.is_open()) {
    RaiseRuntimeError("Cannot open credentials file " + path);
  }
  std::string contents(std::istreambuf_iterator<char>{ifs}, {});
  auto cred_json = nl::json::parse(contents, nullptr, false);
  if (cred_json.is_discarded()) {
    RaiseRuntimeError("Invalid contents in credentials file " + path);
  }
  std::string cred_type = cred_json.value("type", "no type given");
  if (cred_type == "authorized_user") {
    return google::cloud::internal::make_unique<AuthorizedUserCredentials<>>(
        contents, path);
  }
  if (cred_type == "service_account") {
    return google::cloud::internal::make_unique<ServiceAccountCredentials<>>(
        contents, path);
  }
  RaiseRuntimeError(
      "Unsupported credential type (" + cred_type +
      ") when reading Application Default Credentials file from " + path + ".");
}

std::unique_ptr<Credentials> MaybeLoadCredsFromAdcEnvVar() {
  auto path = GoogleAdcFilePathFromEnvVarOrEmpty();
  if (not path.empty()) {
    // If the path was specified, try to load that file; explicitly fail if it
    // doesn't exist or can't be read and parsed.
    return LoadCredsFromPath(path);
  }
  return nullptr;
}

std::unique_ptr<Credentials> MaybeLoadCredsFromGcloudAdcFile() {
  auto path = GoogleAdcFilePathFromWellKnownPathOrEmpty();
  if (not path.empty()) {
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
  return nullptr;
}

std::shared_ptr<Credentials> GoogleDefaultCredentials() {
  // Check if the GOOGLE_APPLICATION_CREDENTIALS environment variable is set.
  auto creds = MaybeLoadCredsFromAdcEnvVar();
  if (creds) {
    return std::move(creds);
  }

  // If no path was specified via environment variable, check if the gcloud
  // ADC file exists.
  creds = MaybeLoadCredsFromGcloudAdcFile();
  if (creds) {
    return std::move(creds);
  }

  // Check for implicit environment-based credentials.

  // TODO(#579): Check if running on App Engine flexible environment.

  // TODO(#579): Check if running on Compute Engine.

  // We've exhausted all search points, thus credentials cannot be constructed.
  std::string adc_link =
      "https://developers.google.com/identity/protocols"
      "/application-default-credentials";
  google::cloud::internal::RaiseRuntimeError(
      "Could not automatically determine credentials. For more information,"
      " please see " +
      adc_link);
}

std::shared_ptr<Credentials> CreateAnonymousCredentials() {
  return std::make_shared<AnonymousCredentials>();
}

std::shared_ptr<Credentials> CreateAuthorizedUserCredentialsFromJsonFilePath(
    std::string const& path) {
  std::ifstream is(path);
  std::string contents(std::istreambuf_iterator<char>{is}, {});
  return std::make_shared<AuthorizedUserCredentials<>>(contents, path);
}

std::shared_ptr<Credentials> CreateAuthorizedUserCredentialsFromJsonContents(
    std::string const& contents) {
  return std::make_shared<AuthorizedUserCredentials<>>(contents, "memory");
}

std::shared_ptr<Credentials> CreateServiceAccountCredentialsFromJsonFilePath(
    std::string const& path) {
  std::ifstream is(path);
  std::string contents(std::istreambuf_iterator<char>{is}, {});
  return std::make_shared<ServiceAccountCredentials<>>(contents, path);
}

std::shared_ptr<Credentials> CreateServiceAccountCredentialsFromJsonContents(
    std::string const& contents) {
  return std::make_shared<ServiceAccountCredentials<>>(contents, "memory");
}

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
