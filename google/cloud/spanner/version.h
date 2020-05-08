// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_VERSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_VERSION_H

#include "google/cloud/spanner/version_info.h"
#include "google/cloud/version.h"
#include <string>

#define SPANNER_CLIENT_NS                              \
  GOOGLE_CLOUD_CPP_VEVAL(SPANNER_CLIENT_VERSION_MAJOR, \
                         SPANNER_CLIENT_VERSION_MINOR)

namespace google {
/**
 * The namespace Google Cloud Platform C++ client libraries.
 */
namespace cloud {
/**
 * Contains all the Cloud Spanner C++ client types and functions.
 */
namespace spanner {
/**
 * The inlined, versioned namespace for the Cloud Spanner C++ client APIs.
 *
 * Applications may need to link multiple versions of the Cloud spanner C++
 * client, for example, if they link a library that uses an older version of
 * the client than they do.  This namespace is inlined, so applications can use
 * `spanner::Foo` in their source, but the symbols are versioned, i.e., the
 * symbol becomes `spanner::v1::Foo`.
 */
inline namespace SPANNER_CLIENT_NS {
/**
 * The Cloud spanner C++ Client major version.
 */
int constexpr VersionMajor() { return SPANNER_CLIENT_VERSION_MAJOR; }

/**
 * The Cloud spanner C++ Client minor version.
 */
int constexpr VersionMinor() { return SPANNER_CLIENT_VERSION_MINOR; }

/**
 * The Cloud spanner C++ Client patch version.
 */
int constexpr VersionPatch() { return SPANNER_CLIENT_VERSION_PATCH; }

/// A single integer representing the Major/Minor/Patch version.
int constexpr Version() {
  static_assert(::google::cloud::version_major() == VersionMajor(),
                "Mismatched major version");
  static_assert(::google::cloud::version_minor() == VersionMinor(),
                "Mismatched minor version");
  static_assert(::google::cloud::version_patch() == VersionPatch(),
                "Mismatched patch version");
  return ::google::cloud::version();
}

/// The version as a string, in MAJOR.MINOR.PATCH+gitrev format.
std::string VersionString();

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_VERSION_H
