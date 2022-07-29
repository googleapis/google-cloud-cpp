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

/**
 * Configure the UserIp query parameter.
 *
 * This can be used to separate quota usage by source IP address.
 *
 * @deprecated prefer using `google::cloud::QuotaUser`.
 */
struct UserIpOption {
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
 * The minimum accepted bytes/second transfer rate.
 *
 * If the average rate is below this value for the `TransferStallTimeoutOption`
 * then the transfer is aborted.
 */
struct TransferStallMinimumRateOption {
  using Type = std::uint32_t;
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

/**
 * The minimum accepted bytes/second download rate.
 *
 * If the average rate is below this value for the `DownloadStallTimeoutOption`
 * then the download is aborted.
 */
struct DownloadStallMinimumRateOption {
  using Type = std::uint32_t;
};

/**
 * Some services appropriate Http error codes for their own use. If any such
 * error codes need to be treated as non-failures, this option can indicate
 * which codes.
 */
struct IgnoredHttpErrorCodes {
  using Type = std::set<std::int32_t>;
};

/// The complete list of options accepted by `CurlRestClient`
using RestOptionList = ::google::cloud::OptionList<
    UserIpOption, TransferStallTimeoutOption, TransferStallMinimumRateOption,
    DownloadStallTimeoutOption, DownloadStallMinimumRateOption,
    IgnoredHttpErrorCodes>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_OPTIONS_H
