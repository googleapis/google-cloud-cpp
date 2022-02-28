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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_OPTIONS_H

#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <chrono>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Configure the UserIp query parameter.
/// For use with GPC services that manage quota per ip address.
struct UserIpOption {
  using Type = std::string;
};

/**
 * Configure the REST endpoint for the GCS client library.
 *
 * This endpoint must include the URL scheme (`http` or `https`) and `authority`
 * (host and port) used to access the GCS service, for example:
 *    https://storage.googleapis.com
 * When using emulators or testbench it can be of the form:
 *    http://localhost:8080/my-gcs-emulator-path
 *
 * @note The `Host` header is based on the `authority` component of the URL.
 *   Applications can override this default value using
 *   `google::cloud::AuthorityOption`
 *
 * @see https://en.wikipedia.org/wiki/Uniform_Resource_Identifier#URLs_and_URNs
 */
struct RestEndpointOption {
  using Type = std::string;
};

/**
 * Sets the transfer stall timeout.
 *
 * If a transfer (upload, download, or request) *stalls*, i.e., no bytes are
 * sent or received for a significant period, it may be better to restart the
 * transfer as this may indicate a network glitch.  For downloads the
 * google::cloud::storage::DownloadStallTimeoutOption takes precedence.
 *
 * For large requests (e.g. downloads in the GiB to TiB range) this is a better
 * configuration parameter than a simple timeout, as the transfers will take
 * minutes or hours to complete. Relying on a timeout value for them would not
 * work, as the timeout would be too large to be useful. For small requests,
 * this is as effective as a timeout parameter, but maybe unfamiliar and thus
 * harder to reason about.
 */
struct TransferStallTimeoutOption {
  using Type = std::chrono::seconds;
};

/**
 * Sets the download stall timeout.
 *
 * If a download *stalls*, i.e., no bytes are received for a significant period,
 * it may be better to restart the download as this may indicate a network
 * glitch.
 *
 * For large requests (e.g. downloads in the GiB to TiB range) this is a better
 * configuration parameter than a simple timeout, as the transfers will take
 * minutes or hours to complete. Relying on a timeout value for them would not
 * work, as the timeout would be too large to be useful. For small requests,
 * this is as effective as a timeout parameter, but maybe unfamiliar and thus
 * harder to reason about.
 */
struct DownloadStallTimeoutOption {
  using Type = std::chrono::seconds;
};

/// The complete list of options accepted by `CurlRestClient`
using RestOptionList =
    ::google::cloud::OptionList<UserIpOption, RestEndpointOption,
                                TransferStallTimeoutOption,
                                DownloadStallTimeoutOption>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_OPTIONS_H
