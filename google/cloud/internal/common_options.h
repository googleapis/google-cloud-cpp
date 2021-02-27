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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_COMMON_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_COMMON_OPTIONS_H

#include "google/cloud/version.h"
#include <set>
#include <string>
#include <tuple>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/**
 * Change the endpoint.
 *
 * In almost all cases a suitable default will be chosen automatically.
 * Applications may need to be changed to (1) test against a fake or simulator,
 * or (2) use a beta or EAP version of the service.
 */
struct EndpointOption {
  using Type = std::string;
};

/**
 * User-agent strings to include with each request.
 *
 * Libraries or services that use Cloud C++ clients may want to set their own
 * user-agent prefix. This can help them develop telemetry information about
 * number of users running particular versions of their system or library.
 */
struct UserAgentPrefixOption {
  using Type = std::set<std::string>;
};

/**
 * Return whether tracing is enabled for the given @p component.
 *
 * The C++ clients can log interesting events to help library and application
 * developers troubleshoot problems. To see log messages (maybe lots) you can
 * enable tracing for the component that interests you. Valid components are
 * currently:
 *
 * - rpc
 * - rpc-streams
 */
struct TracingComponentsOption {
  using Type = std::set<std::string>;
};

}  // namespace internal

namespace internal {

/**
 * A list of all the options in this file.
 *
 * This is intended to be used with `internal::CheckExpectedOptions<T>()` to
 * make it easy to specify groups of options as allowed/expected.
 *
 * @code
 * Options opts;
 * opts.set<EndpointOption>("...");
 * internal::CheckExpectedOptions<internal::CommonOptions>(
 *     opts, "some factory function");
 * @endcode
 */
using CommonOptions =
    std::tuple<EndpointOption, UserAgentPrefixOption, TracingComponentsOption>;
}  // namespace internal

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_COMMON_OPTIONS_H
