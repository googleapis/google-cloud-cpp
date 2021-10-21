// Copyright 2017 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_VERSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_VERSION_H

#include "google/cloud/bigtable/version_info.h"
#include "google/cloud/internal/attributes.h"
#include "google/cloud/version.h"
#include <string>

#define GOOGLE_CLOUD_CPP_BIGTABLE_IAM_DEPRECATED(alternative)                  \
  GOOGLE_CLOUD_CPP_DEPRECATED(                                                 \
      "this function predates IAM conditions and does not work with policies " \
      "that include IAM conditions. Please use " alternative                   \
      " instead. The function will be removed on 2022-04-01 or shortly "       \
      "after. See GitHub issue #5929 for more information.")

#define BIGTABLE_CLIENT_NS                              \
  GOOGLE_CLOUD_CPP_VEVAL(BIGTABLE_CLIENT_VERSION_MAJOR, \
                         BIGTABLE_CLIENT_VERSION_MINOR)

namespace google {
namespace cloud {
/**
 * Contains all the Cloud Bigtable C++ client APIs.
 */
namespace bigtable {
/**
 * The inlined, versioned namespace for the Cloud Bigtable C++ client APIs.
 *
 * Applications may need to link multiple versions of the Cloud Bigtable C++
 * client, for example, if they link a library that uses an older version of
 * the client than they do.  This namespace is inlined, so applications can use
 * `bigtable::Foo` in their source, but the symbols are versioned, i.e., the
 * symbol becomes `bigtable::v1::Foo`.
 *
 * Note that, consistent with the semver.org guidelines, the v0 version makes
 * no guarantees with respect to backwards compatibility.
 */
inline namespace BIGTABLE_CLIENT_NS {
/**
 * The Cloud Bigtable C++ Client major version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
int constexpr version_major() { return BIGTABLE_CLIENT_VERSION_MAJOR; }

/**
 * The Cloud Bigtable C++ Client minor version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
int constexpr version_minor() { return BIGTABLE_CLIENT_VERSION_MINOR; }

/**
 * The Cloud Bigtable C++ Client patch version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
int constexpr version_patch() { return BIGTABLE_CLIENT_VERSION_PATCH; }

/// A single integer representing the Major/Minor/Patch version.
int constexpr version() {
  static_assert(::google::cloud::version_major() == version_major(),
                "Mismatched major version");
  static_assert(::google::cloud::version_minor() == version_minor(),
                "Mismatched minor version");
  static_assert(::google::cloud::version_patch() == version_patch(),
                "Mismatched patch version");
  return ::google::cloud::version();
}

/// The version as a string, in MAJOR.MINOR.PATCH+gitrev format.
std::string version_string();

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_VERSION_H
