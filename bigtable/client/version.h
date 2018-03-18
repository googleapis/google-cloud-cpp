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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_VERSION_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_VERSION_H_

#include "bigtable/client/internal/port_platform.h"
#include "bigtable/client/version_info.h"
#include <string>

#define BIGTABLE_CLIENT_VCONCAT(Ma, Mi) v##Ma
#define BIGTABLE_CLIENT_VEVAL(Ma, Mi) BIGTABLE_CLIENT_VCONCAT(Ma, Mi)
#define BIGTABLE_CLIENT_NS                             \
  BIGTABLE_CLIENT_VEVAL(BIGTABLE_CLIENT_VERSION_MAJOR, \
                        BIGTABLE_CLIENT_VERSION_MINOR)

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
  return 100 * (100 * version_major() + version_minor()) + version_patch();
}

/// The version as a string, in MAJOR.MINOR.PATCH+gitrev format.
std::string version_string();

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_VERSION_H_
