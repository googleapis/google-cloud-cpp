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
 * Returns the environment variable that should be checked for a valid file
 * path when attempting to load Google Application Default Credentials.
 */
inline char const* GoogleAdcEnvVar() {
  static constexpr char kEnvVarName[] = "GOOGLE_APPLICATION_CREDENTIALS";
  return kEnvVarName;
}

/**
 * Returns the path to the file containing Application Default Credentials, if
 * set in the GOOGLE_APPLICATION_CREDENTIALS environment variable. Returns an
 * empty string if no such path exists.
 */
std::string GoogleAdcFilePathOrEmpty();

/**
 * Returns the environment variable that should be used to indicate the
 * directory where the user's application configuration data is stored, which is
 * used when constructing the well known path of the Google Application Default
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
