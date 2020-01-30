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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_VERSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_VERSION_H

#include "google/cloud/internal/port_platform.h"
#include "google/cloud/internal/version_info.h"
#include <string>

#define GOOGLE_CLOUD_CPP_VCONCAT(Ma, Mi) v##Ma
#define GOOGLE_CLOUD_CPP_VEVAL(Ma, Mi) GOOGLE_CLOUD_CPP_VCONCAT(Ma, Mi)
#define GOOGLE_CLOUD_CPP_NS                              \
  GOOGLE_CLOUD_CPP_VEVAL(GOOGLE_CLOUD_CPP_VERSION_MAJOR, \
                         GOOGLE_CLOUD_CPP_VERSION_MINOR)

namespace google {
/**
 * Contains all the Google Cloud C++ Library APIs.
 */
namespace cloud {
/**
 * The Google Cloud C++ Library inlined, versioned namespace.
 *
 * Applications may need to link multiple versions of the Google Cloud C++
 * Libraries, for example, if they link a library that uses an older version of
 * the libraries than they do.  This namespace is inlined, so applications can
 * use `google::cloud::Foo` in their source, but the symbols are versioned,
 * i.e., the symbol becomes `google::cloud::v1::Foo`.
 *
 * Note that, consistent with the semver.org guidelines, the v0 version makes
 * no guarantees with respect to backwards compatibility.
 */
inline namespace GOOGLE_CLOUD_CPP_NS {
/**
 * The Google Cloud Storage C++ Client major version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
int constexpr version_major() { return GOOGLE_CLOUD_CPP_VERSION_MAJOR; }

/**
 * The Google Cloud Storage C++ Client minor version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
int constexpr version_minor() { return GOOGLE_CLOUD_CPP_VERSION_MINOR; }

/**
 * The Google Cloud Storage C++ Client patch version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
int constexpr version_patch() { return GOOGLE_CLOUD_CPP_VERSION_PATCH; }

/// A single integer representing the Major/Minor/Patch version.
int constexpr version() {
  return 100 * (100 * version_major() + version_minor()) + version_patch();
}

/// The version as a string, in MAJOR.MINOR.PATCH+gitrev format.
std::string version_string();

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_VERSION_H
