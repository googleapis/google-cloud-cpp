// Copyright 2017 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_VERSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_VERSION_H

#include "google/cloud/bigtable/version_info.h"
#include "google/cloud/version.h"
#include <string>

#define GOOGLE_CLOUD_CPP_BIGTABLE_DATA_CLIENT_DEPRECATED(name)              \
  GOOGLE_CLOUD_CPP_DEPRECATED(                                              \
      "google::cloud::bigtable::DataClient::" name                          \
      " is deprecated, and will be removed on or shortly after 2023-05-01." \
      " See GitHub issue #8800 for more information.")

#define GOOGLE_CLOUD_CPP_BIGTABLE_ROW_READER_CTOR_DEPRECATED()             \
  GOOGLE_CLOUD_CPP_DEPRECATED(                                             \
      "RowReader public constructors have been deprecated. They are "      \
      " scheduled for deletion on 2023-05-01. Please use the `RowReader` " \
      "returned by `Table::ReadRows()`.")

// This preprocessor symbol is deprecated and should never be used anywhere. It
// exists solely for backward compatibility to avoid breaking anyone who may
// have been using it.
#define BIGTABLE_CLIENT_NS GOOGLE_CLOUD_CPP_NS

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The Cloud Bigtable C++ Client major version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
int constexpr version_major() { return google::cloud::version_major(); }

/**
 * The Cloud Bigtable C++ Client minor version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
int constexpr version_minor() { return google::cloud::version_minor(); }

/**
 * The Cloud Bigtable C++ Client patch version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
int constexpr version_patch() { return google::cloud::version_patch(); }

/**
 * The Cloud Bigtable C++ Client pre-release version.
 *
 * @see https://semver.org/spec/v2.0.0.html for details.
 */
constexpr char const* version_pre_release() {
  return google::cloud::version_pre_release();
}

/// A single integer representing the Major/Minor/Patch version.
int constexpr version() { return google::cloud::version(); }

/// The version as a string, in MAJOR.MINOR.PATCH[-PRE][+gitrev] format.
std::string version_string();

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
// TODO(#7463) - remove backwards compatibility namespaces
namespace v1 = GOOGLE_CLOUD_CPP_NS;  // NOLINT(misc-unused-alias-decls)
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_VERSION_H
