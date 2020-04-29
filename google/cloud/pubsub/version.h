// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_VERSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_VERSION_H

#include "google/cloud/pubsub/version_info.h"
#include "google/cloud/version.h"
#include <string>

#define GOOGLE_CLOUD_CPP_PUBSUB_NS                                     \
  GOOGLE_CLOUD_CPP_VEVAL(GOOGLE_CLOUD_CPP_PUBSUB_CLIENT_VERSION_MAJOR, \
                         GOOGLE_CLOUD_CPP_PUBSUB_CLIENT_VERSION_MINOR)

namespace google {
/**
 * The namespace Google Cloud Platform C++ client libraries.
 */
namespace cloud {
/**
 * Contains all the Cloud Pubsub C++ client types and functions.
 */
namespace pubsub {
/**
 * The inlined, versioned namespace for the Cloud Pubsub C++ client APIs.
 *
 * Applications may need to link multiple versions of the Cloud pubsub C++
 * client, for example, if they link a library that uses an older version of
 * the client than they do.  This namespace is inlined, so applications can use
 * `pubsub::Foo` in their source, but the symbols are versioned, i.e., the
 * symbol becomes `pubsub::v1::Foo`.
 */
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
/**
 * The Cloud pubsub C++ Client major version.
 */
int constexpr VersionMajor() {
  return GOOGLE_CLOUD_CPP_PUBSUB_CLIENT_VERSION_MAJOR;
}

/**
 * The Cloud pubsub C++ Client minor version.
 */
int constexpr VersionMinor() {
  return GOOGLE_CLOUD_CPP_PUBSUB_CLIENT_VERSION_MINOR;
}

/**
 * The Cloud pubsub C++ Client patch version.
 */
int constexpr VersionPatch() {
  return GOOGLE_CLOUD_CPP_PUBSUB_CLIENT_VERSION_PATCH;
}

auto constexpr kMaxPatchVersions = 100;
auto constexpr kMaxMinorVersions = 100;

/// A single integer representing the Major/Minor/Patch version.
int constexpr Version() {
  return kMaxPatchVersions *
             (kMaxMinorVersions * VersionMajor() + VersionMinor()) +
         VersionPatch();
}

/// The version as a string, in MAJOR.MINOR.PATCH+gitrev format.
std::string VersionString();

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_VERSION_H
