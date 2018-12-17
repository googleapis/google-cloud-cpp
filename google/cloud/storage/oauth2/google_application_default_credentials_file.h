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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_GOOGLE_APPLICATION_DEFAULT_CREDENTIALS_FILE_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_GOOGLE_APPLICATION_DEFAULT_CREDENTIALS_FILE_H_

#include "google/cloud/storage/version.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {

/**
 * Returns the Application Default %Credentials environment variable name.
 *
 * This environment variable should be checked for a valid file path when
 * attempting to load Google Application Default %Credentials.
 */
inline char const* GoogleAdcEnvVar() {
  static constexpr char kEnvVarName[] = "GOOGLE_APPLICATION_CREDENTIALS";
  return kEnvVarName;
}

/**
 * Returns the path to the Application Default %Credentials file, if set.
 *
 * If the Application Default %Credentials environment variable is set, we check
 * the path specified by its value for a file containing ADCs. Returns an
 * empty string if no such path exists or the environment variable is not set.
 */
std::string GoogleAdcFilePathFromEnvVarOrEmpty();

/**
 * Returns the path to the Application Default %Credentials file, if set.
 *
 * If the gcloud utility has configured an Application Default %Credentials
 * file, the path to that file is returned. Returns an empty string if no such
 * file exists at the well known path.
 */
std::string GoogleAdcFilePathFromWellKnownPathOrEmpty();

/**
 * Returns the environment variable to override the gcloud ADC path.
 *
 * This environment variable is used for testing to override the path that
 * should be searched for the gcloud Application Default %Credentials file.
 */
inline char const* GoogleGcloudAdcFileEnvVar() {
  static constexpr char kEnvVarName[] = "GOOGLE_GCLOUD_ADC_PATH_OVERRIDE";
  return kEnvVarName;
}

/**
 * Returns the environment variable used to construct the well known ADC path.
 *
 * The directory containing a user's application configuration data, indicated
 * by this environment variable, varies across environments. That directory is
 * used when constructing the well known path of the Application Default
 * Credentials file.
 */
inline char const* GoogleAdcHomeEnvVar() {
#ifdef _WIN32
  static constexpr char kHomeEnvVar[] = "APPDATA";
#else
  static constexpr char kHomeEnvVar[] = "HOME";
#endif
  return kHomeEnvVar;
}

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_GOOGLE_APPLICATION_DEFAULT_CREDENTIALS_FILE_H_
