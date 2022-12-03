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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_VERSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_VERSION_H

#include "google/cloud/storage/version_info.h"
#include "google/cloud/internal/attributes.h"
#include "google/cloud/version.h"
#include <string>

#if defined(_MSC_VER) && _MSC_VER < 1929
#define GOOGLE_CLOUD_CPP_STORAGE_RESTORE_UPLOAD_DEPRECATED() /**/
#else
#define GOOGLE_CLOUD_CPP_STORAGE_RESTORE_UPLOAD_DEPRECATED()                   \
  GOOGLE_CLOUD_CPP_DEPRECATED(                                                 \
      "this function is not used within the library. There was never a need"   \
      " to mock this function, but it is preserved to avoid breaking existing" \
      " applications. The function may be removed after 2022-10-01, more"      \
      " details on GitHub issue #7282.")
#endif  // _MSC_VER

// This preprocessor symbol is deprecated and should never be used anywhere. It
// exists solely for backward compatibility to avoid breaking anyone who may
// have been using it.
#define STORAGE_CLIENT_NS GOOGLE_CLOUD_CPP_NS

namespace google {
namespace cloud {
/**
 * Contains all the Google Cloud Storage C++ client APIs.
 */
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Returns the Google Cloud Storage C++ Client major version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
int constexpr version_major() { return google::cloud::version_major(); }

/**
 * Returns the Google Cloud Storage C++ Client minor version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
int constexpr version_minor() { return google::cloud::version_minor(); }

/**
 * Returns the Google Cloud Storage C++ Client patch version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
int constexpr version_patch() { return google::cloud::version_patch(); }

/**
 * Returns the Google Cloud Storage C++ Client pre-release version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
constexpr char const* version_pre_release() {
  return google::cloud::version_pre_release();
}

/// Returns a single integer representing the Major/Minor/Patch version.
int constexpr version() { return google::cloud::version(); }

/// Returns the version as a string, in MAJOR.MINOR.PATCH[-PRE][+gitrev] format.
std::string version_string();

/// Returns the value for `x-goog-api-client` header.
std::string x_goog_api_client();

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
// TODO(#7463) - remove backwards compatibility namespaces
namespace v1 = GOOGLE_CLOUD_CPP_NS;  // NOLINT(misc-unused-alias-decls)
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_VERSION_H
