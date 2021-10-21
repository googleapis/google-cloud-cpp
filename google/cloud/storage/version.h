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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_VERSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_VERSION_H

#include "google/cloud/storage/version_info.h"
#include "google/cloud/internal/attributes.h"
#include "google/cloud/version.h"
#include <string>

#define GOOGLE_CLOUD_CPP_STORAGE_IAM_DEPRECATED(alternative)                   \
  GOOGLE_CLOUD_CPP_DEPRECATED(                                                 \
      "this function predates IAM conditions and does not work with policies " \
      "that include IAM conditions. Please use " alternative                   \
      " instead. The function will be removed on 2022-04-01 or shortly "       \
      "after. See GitHub issue #5929 for more information.")

#define STORAGE_CLIENT_NS                              \
  GOOGLE_CLOUD_CPP_VEVAL(STORAGE_CLIENT_VERSION_MAJOR, \
                         STORAGE_CLIENT_VERSION_MINOR)

namespace google {
namespace cloud {
/**
 * Contains all the Google Cloud Storage C++ client APIs.
 */
namespace storage {
/**
 * The Google Cloud Storage C++ client APIs inlined, versioned namespace.
 *
 * Applications may need to link multiple versions of the Google Cloud Storage
 * C++ client, for example, if they link a library that uses an older version of
 * the client than they do.  This namespace is inlined, so applications can use
 * `storage::Foo` in their source, but the symbols are versioned, i.e., the
 * symbol becomes `storage::v1::Foo`.
 *
 * Note that, consistent with the semver.org guidelines, the v0 version makes
 * no guarantees with respect to backwards compatibility.
 */
inline namespace STORAGE_CLIENT_NS {
/**
 * Returns the Google Cloud Storage C++ Client major version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
int constexpr version_major() { return STORAGE_CLIENT_VERSION_MAJOR; }

/**
 * Returns the Google Cloud Storage C++ Client minor version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
int constexpr version_minor() { return STORAGE_CLIENT_VERSION_MINOR; }

/**
 * Returns the Google Cloud Storage C++ Client patch version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
int constexpr version_patch() { return STORAGE_CLIENT_VERSION_PATCH; }

/// Returns a single integer representing the Major/Minor/Patch version.
int constexpr version() {
  static_assert(::google::cloud::version_major() == version_major(),
                "Mismatched major version");
  static_assert(::google::cloud::version_minor() == version_minor(),
                "Mismatched minor version");
  static_assert(::google::cloud::version_patch() == version_patch(),
                "Mismatched patch version");
  return ::google::cloud::version();
}

/// Returns the version as a string, in MAJOR.MINOR.PATCH+gitrev format.
std::string version_string();

/// Returns the value for `x-goog-api-client` header.
std::string x_goog_api_client();

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_VERSION_H
