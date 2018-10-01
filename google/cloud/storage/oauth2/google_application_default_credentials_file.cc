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

#include "google/cloud/storage/oauth2/google_application_default_credentials_file.h"
#include <cstdlib>

namespace {

std::string const& GoogleWellKnownAdcFilePathSuffix() {
#ifdef _WIN32
  static std::string const suffix =
      "/gcloud/application_default_credentials.json";
#else
  static std::string const suffix =
      "/.config/gcloud/application_default_credentials.json";
#endif
  return suffix;
}

}  // anonymous namespace

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {

std::string GoogleAdcFilePathOrEmpty() {
  auto override_value = std::getenv(GoogleAdcEnvVar());
  if (override_value != nullptr) {
    return std::string(override_value);
  }
  // Search well known gcloud ADC path.
  auto adc_path_root = std::getenv(GoogleAdcHomeEnvVar());
  if (adc_path_root != nullptr) {
    return std::string(adc_path_root) + GoogleWellKnownAdcFilePathSuffix();
  }
  return "";
}

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
