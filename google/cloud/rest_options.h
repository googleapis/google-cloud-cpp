// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_REST_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_REST_OPTIONS_H

#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Configure the QuotaUser system parameter.
 *
 * A pseudo user identifier for charging per-user quotas. If not specified, the
 * authenticated principal is used. If there is no authenticated principal, the
 * client IP address will be used. When specified, a valid API key with service
 * restrictions must be used to identify the quota project. Otherwise, this
 * parameter is ignored.
 *
 * @ingroup rest-options
 */
struct QuotaUserOption {
  using Type = std::string;
};

/**
 * Configure the UserIp query parameter.
 *
 * This can be used to separate quota usage by source IP address.
 *
 * @deprecated prefer using `google::cloud::QuotaUser`.
 * @ingroup rest-options
 */
struct UserIpOption {
  using Type = std::string;
};

/**
 * Timeout (in seconds) for the server to finish processing the request. This
 * system param only applies to REST APIs for which client-side timeout is not
 * applicable.
 * @ingroup rest-options
 */
struct ServerTimeoutOption {
  using Type = double;
};

/// The complete list of options accepted by `CurlRestClient`
using RestOptionList =
    ::google::cloud::OptionList<QuotaUserOption, ServerTimeoutOption,
                                UserIpOption>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_REST_OPTIONS_H
