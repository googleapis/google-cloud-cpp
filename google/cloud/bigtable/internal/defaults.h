// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_DEFAULTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_DEFAULTS_H

#include "google/cloud/bigtable/version.h"
#include "google/cloud/options.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

int DefaultConnectionPoolSize();

/**
 * Returns an `Options` with the appropriate defaults for Bigtable.
 *
 * Environment variables and the optional @p opts argument may be consulted to
 * determine the correct `Options` to set. It's up to the implementation as to
 * what overrides what. For example, it may be that a user-provided value for
 * `DataEndpointOption` via @p opts takes precedence, OR it may be that an
 * environment variable overrides that, and these rules may differ for each
 * setting.
 *
 * Option values that this implementation doesn't know about will be passed
 * along unmodified.
 */
Options DefaultOptions(Options opts = {});

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_DEFAULTS_H
