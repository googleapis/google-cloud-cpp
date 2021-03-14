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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_COMMON_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_COMMON_OPTIONS_H

#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <set>
#include <string>
#include <vector>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {

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
 * User-agent products to include with each request.
 *
 * Libraries or services that use Cloud C++ clients may want to set their own
 * user-agent product information. This can help them develop telemetry
 * information about number of users running particular versions of their
 * system or library.
 *
 * @see https://tools.ietf.org/html/rfc7231#section-5.5.3
 */
struct UserAgentProductsOption {
  using Type = std::vector<std::string>;
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

/**
 * A list of all the common options.
 */
using CommonOptionList = OptionList<EndpointOption, UserAgentProductsOption,
                                    TracingComponentsOption>;

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_COMMON_OPTIONS_H
