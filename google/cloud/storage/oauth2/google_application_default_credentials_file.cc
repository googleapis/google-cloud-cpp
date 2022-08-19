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

#include "google/cloud/storage/oauth2/google_application_default_credentials_file.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/oauth2_google_application_default_credentials_file.h"
#include <cstdlib>

namespace {

std::string const& GoogleWellKnownAdcFilePathSuffix() {
#ifdef _WIN32
  static auto const* const kSuffix =
      new std::string("/gcloud/application_default_credentials.json");
#else
  static auto const* const kSuffix =
      new std::string("/.config/gcloud/application_default_credentials.json");
#endif
  return *kSuffix;
}

}  // anonymous namespace

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace oauth2 {

char const* GoogleAdcEnvVar() {
  return google::cloud::oauth2_internal::GoogleAdcEnvVar();
}

std::string GoogleAdcFilePathFromEnvVarOrEmpty() {
  auto override_value = google::cloud::internal::GetEnv(GoogleAdcEnvVar());
  if (override_value.has_value()) {
    return *override_value;
  }
  return "";
}

std::string GoogleAdcFilePathFromWellKnownPathOrEmpty() {
  // Allow mocking out this value for testing.
  auto override_path =
      google::cloud::internal::GetEnv(GoogleGcloudAdcFileEnvVar());
  if (override_path.has_value()) {
    return *override_path;
  }

  // Search well known gcloud ADC path.
  auto adc_path_root = google::cloud::internal::GetEnv(GoogleAdcHomeEnvVar());
  if (adc_path_root.has_value()) {
    return *adc_path_root + GoogleWellKnownAdcFilePathSuffix();
  }
  return "";
}

char const* GoogleGcloudAdcFileEnvVar() {
  return google::cloud::oauth2_internal::GoogleGcloudAdcFileEnvVar();
}

char const* GoogleAdcHomeEnvVar() {
  return google::cloud::oauth2_internal::GoogleAdcHomeEnvVar();
}

}  // namespace oauth2
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
