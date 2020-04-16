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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_INTERNAL_BUILD_INFO_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_INTERNAL_BUILD_INFO_H

#include "google/cloud/bigquery/version.h"

namespace google {
namespace cloud {
namespace bigquery {
inline namespace BIGQUERY_CLIENT_NS {
namespace internal {

/**
 * Returns the build flags.
 *
 * Examples include "-c fastbuild" or "-O2 -DNDEBUG".
 */
std::string BuildFlags();

/**
 * Returns the metadata injected by the build system.
 *
 * See https://semver.org/#spec-item-10 for more details about the use and
 * format of build metadata. Typically, the the value returned here is a hash
 * indicating a git commit.
 */
std::string BuildMetadata();

}  // namespace internal
}  // namespace BIGQUERY_CLIENT_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_INTERNAL_BUILD_INFO_H
