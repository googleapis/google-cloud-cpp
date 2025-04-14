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

#include "google/cloud/common_options.h"
#include "google/cloud/options.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include <chrono>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace experimental {

/**
 * Function signature for the libcurl SSL context callback.
 *
 * This signature matches the prototype declared by libcurl, but its invocation
 * is wrapped by the Cloud C++ SDK. This is a precaution to prevent the CURL
 * handle from being altered in ways that would cause the SDK to malfunction.
 *
 * The callback should return CURLE_OK on success and CURLE_ABORTED_BY_CALLBACK
 * on error.
 *
 * @note While the callback defines three pointer parameters, only the ssl_ctx
 * pointer will have a non-NULL value when the callback is called.
 */
using SslCtxCallback = std::function<int(void*, void* ssl_ctx, void*)>;

/**
 * This option allows the user to specify a function that is registered with
 * libcurl as the CURLOPT_SSL_CTX_FUNCTION.
 *
 * @note This is an advanced option and should only be used when other options
 * such as:
 *   - CAInMemoryOption
 *   - CAPathOption
 *   - CARootsFilePathOption
 *   - ClientSslCertificateOption
 * are insufficient.
 *
 * @note Setting this option causes the following Options to be ignored:
 *   - CAInMemoryOption
 *   - CAPathOption
 *   - CARootsFilePathOption
 *
 * @note This Option is not currently supported on Windows.
 * @note This Option requires libcurl 7.10.6 or higher.
 */
struct SslCtxCallbackOption {
  using Type = SslCtxCallback;
};

}  // namespace experimental

/**
 * Timeout for the server to finish processing the request. This system param
 * only applies to REST APIs for which client-side timeout is not applicable.
 *
 * @ingroup rest-options
 */
struct ServerTimeoutOption {
  using Type = std::chrono::milliseconds;
};

/**
 * The `TracingOptions` to use when printing REST transport http messages.
 *
 * @ingroup options
 */
struct RestTracingOptionsOption {
  using Type = TracingOptions;
};

/**
 * Sets the interface name to use as outgoing network interface. The
 * name can be an interface name, IP address, or hostname. To
 * utilize one of these use the following special prefixes:
 *
 * if![name] for interface name, host![name] for IP address or hostname,
 * ifhost![interface]![host] for interface name and IP address or hostname.
 *
 * The default is to use whatever the TCP stack finds suitable.
 */
struct Interface {
  using Type = std::string;
};

/// The complete list of options accepted by `CurlRestClient`
using RestOptionList =
    ::google::cloud::OptionList<QuotaUserOption, RestTracingOptionsOption,
                                ServerTimeoutOption, UserIpOption, Interface>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_REST_OPTIONS_H
