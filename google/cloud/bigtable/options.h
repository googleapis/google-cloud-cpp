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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_OPTIONS_H

/**
 * @file
 *
 * This file defines options to be used with instances of
 * `google::cloud::Options`. By convention options are named with an "Option"
 * suffix. As the name would imply, all options are optional, and leaving them
 * unset will result in a reasonable default being chosen.
 *
 * Not all options are meaningful to all functions that accept a
 * `google::cloud::Options` instance. Each function that accepts a
 * `google::cloud::Options` should document which options it expects. This is
 * typically done by indicating lists of options using "OptionList" aliases.
 * For example, a function may indicate that users may set any option in
 * `ClientOptionList`.
 *
 * @note Unrecognized options are allowed and will be ignored. To debug issues
 *     with options set `GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes` in the environment
 *     and unexpected options will be logged.
 *
 * @see `google::cloud::CommonOptionList`
 * @see `google::cloud::GrpcOptionList`
 */

#include "google/cloud/bigtable/version.h"
#include "google/cloud/options.h"
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {

/// The endpoint for data operations.
struct DataEndpointOption {
  using Type = std::string;
};

/// The endpoint for table admin operations.
struct AdminEndpointOption {
  using Type = std::string;
};

/**
 * The endpoint for instance admin operations.
 *
 * In most scenarios this should have the same value as `AdminEndpointOption`.
 * The most common exception is testing, where the emulator for instance admin
 * operations may be different than the emulator for admin and data operations.
 */
struct InstanceAdminEndpointOption {
  using Type = std::string;
};

/**
 * Minimum time in ms to refresh connections.
 *
 * The server wll not disconnect idle connections before this time.
 */
struct MinConnectionRefreshOption {
  using Type = std::chrono::milliseconds;
};

/**
 * Maximum time in ms to refresh connections.
 *
 * The server will disconnect idle connections before this time.
 */
struct MaxConnectionRefreshOption {
  using Type = std::chrono::milliseconds;
};

/// The complete list of options accepted by `bigtable::*Client`
using ClientOptionList =
    OptionList<DataEndpointOption, AdminEndpointOption,
               InstanceAdminEndpointOption, MinConnectionRefreshOption,
               MaxConnectionRefreshOption>;

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_OPTIONS_H
