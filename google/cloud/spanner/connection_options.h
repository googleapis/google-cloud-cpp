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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_CONNECTION_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_CONNECTION_OPTIONS_H

#include "google/cloud/spanner/version.h"
#include "google/cloud/connection_options.h"
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * The traits to configure `ConnectionOptions<T>` for Cloud Spanner.
 *
 * See `google::cloud::ConnectionOptions<T>` for more details at
 * https://googleapis.dev/cpp/google-cloud-common/latest/
 */
struct ConnectionOptionsTraits {
  static std::string default_endpoint();
  static std::string user_agent_prefix();
  static int default_num_channels();
};

/**
 * The options for Cloud Spanner connections.
 *
 * See `google::cloud::ConnectionOptions<T>` for more details at
 * https://googleapis.dev/cpp/google-cloud-common/latest/
 */
using ConnectionOptions =
    google::cloud::ConnectionOptions<ConnectionOptionsTraits>;

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner

namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
spanner::ConnectionOptions EmulatorOverrides(
    spanner::ConnectionOptions options);
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal

}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_CONNECTION_OPTIONS_H
