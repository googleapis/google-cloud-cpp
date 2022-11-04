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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_VERSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_VERSION_H

#include "google/cloud/internal/attributes.h"
#include "google/cloud/internal/port_platform.h"
#include "google/cloud/internal/version_info.h"
#include <string>

#define GOOGLE_CLOUD_CPP_VCONCAT(Ma, Mi, Pa) v##Ma##_##Mi##_##Pa
#define GOOGLE_CLOUD_CPP_VEVAL(Ma, Mi, Pa) GOOGLE_CLOUD_CPP_VCONCAT(Ma, Mi, Pa)
#define GOOGLE_CLOUD_CPP_NS                              \
  GOOGLE_CLOUD_CPP_VEVAL(GOOGLE_CLOUD_CPP_VERSION_MAJOR, \
                         GOOGLE_CLOUD_CPP_VERSION_MINOR, \
                         GOOGLE_CLOUD_CPP_VERSION_PATCH)

/**
 * Versioned inline namespace that users should generally avoid spelling.
 *
 * The actual inline namespace name will change with each release, and if you
 * use it your code will be tightly coupled to a specific release. Omitting the
 * inline namespace name will make upgrading to newer releases easier.
 *
 * However, applications may need to link multiple versions of the Google Cloud
 * C++ Libraries, for example, if they link a library that uses an older
 * version of the libraries than they do. This namespace is inlined, so
 * applications can use `google::cloud::Foo` in their source, but the symbols
 * are versioned, i.e., the symbol becomes `google::cloud::vXYZ::Foo`.
 */
#define GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN \
  inline namespace GOOGLE_CLOUD_CPP_NS {
#define GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END \
  } /* namespace GOOGLE_CLOUD_CPP_NS */

// This preprocessor symbol is deprecated and should never be used anywhere. It
// exists solely for backward compatibility to avoid breaking anyone who may
// have been using it.
#define GOOGLE_CLOUD_CPP_GENERATED_NS GOOGLE_CLOUD_CPP_NS

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The Google Cloud C++ Client major version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
int constexpr version_major() { return GOOGLE_CLOUD_CPP_VERSION_MAJOR; }

/**
 * The Google Cloud C++ Client minor version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
int constexpr version_minor() { return GOOGLE_CLOUD_CPP_VERSION_MINOR; }

/**
 * The Google Cloud C++ Client patch version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
int constexpr version_patch() { return GOOGLE_CLOUD_CPP_VERSION_PATCH; }

/**
 * The Google Cloud C++ Client pre-release version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
constexpr char const* version_pre_release() {
  return GOOGLE_CLOUD_CPP_PRE_RELEASE;
}

namespace internal {
auto constexpr kMaxMinorVersions = 100;
auto constexpr kMaxPatchVersions = 100;
}  // namespace internal

/// A single integer representing the Major/Minor/Patch version.
int constexpr version() {
  static_assert(version_minor() < internal::kMaxMinorVersions,
                "version_minor() should be < kMaxMinorVersions");
  static_assert(version_patch() < internal::kMaxPatchVersions,
                "version_patch() should be < kMaxPatchVersions");
  return internal::kMaxPatchVersions *
             (internal::kMaxMinorVersions * version_major() + version_minor()) +
         version_patch();
}

/// The version as a string, in MAJOR.MINOR.PATCH[-PRE][+gitrev] format.
std::string version_string();

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
// TODO(#7463) - remove backwards compatibility namespaces
namespace v1 = GOOGLE_CLOUD_CPP_NS;        // NOLINT(misc-unused-alias-decls)
namespace gcpcxxV1 = GOOGLE_CLOUD_CPP_NS;  // NOLINT(misc-unused-alias-decls)
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_VERSION_H
