// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_OPTIONS_H

#include "google/cloud/options.h"
#include <string>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Set the HTTP version used by the client.
 *
 * If this option is not provided, or is set to `default` then the library uses
 * [libcurl's default], typically HTTP/2 with SSL. Possible settings include:
 * - "1.0": use HTTP/1.0, this is not recommended as would require a new
 *   connection for each request.
 * - "1.1": use HTTP/1.1, this may be useful when the overhead of HTTP/2 is
 *   unacceptable. Note that this may require additional connections.
 * - "2TLS": use HTTP/2 with TLS
 * - "2.0": use HTTP/2 with our without TLS.
 *
 * [libcurl's default]: https://curl.se/libcurl/c/CURLOPT_HTTP_VERSION.html
 */
struct HttpVersionOption {
  using Type = std::string;
};

/// This is only intended for testing. It is not for public use.
struct CAPathOption {
  using Type = std::string;
};

/**
 * Set the maximum connection pool size.
 *
 * The C++ client library uses this value to limit the growth of the
 * connection pool. Once an operation (a RPC or a download) completes the
 * connection used for that operation is returned to the pool. If the pool is
 * full the connection is immediately released. If the pool has room the
 * connection is cached for the next RPC or download.
 *
 * @note The behavior of this pool may change in the future, depending on the
 * low-level implementation details of the library.
 *
 * @note The library does not create connections proactively, setting a high
 * value may result in very few connections if your application does not need
 * them.
 *
 * @note Setting this value to 0 disables connection pooling.
 *
 * @warning The library may create more connections than this option configures,
 * for example if your application requests many simultaneous downloads.
 */
struct ConnectionPoolSizeOption {
  using Type = std::size_t;
};

/**
 * Disables automatic OpenSSL locking.
 *
 * With older versions of OpenSSL any locking must be provided by locking
 * callbacks in the application or intermediate libraries. The C++ client
 * library automatically provides the locking callbacks. If your application
 * already provides such callbacks, and you prefer to use them, set this option
 * to `false`.
 *
 * @note This option is only useful for applications linking against
 * OpenSSL 1.0.2.
 */
struct EnableCurlSslLockingOption {
  using Type = bool;
};

/**
 * Disables automatic OpenSSL sigpipe handler.
 *
 * With some versions of OpenSSL it might be necessary to setup a SIGPIPE
 * handler. If your application already provides such a handler, set this option
 * to `false` to disable the handler in the GCS C++ client library.
 */
struct EnableCurlSigpipeHandlerOption {
  using Type = bool;
};

/**
 * Control the maximum socket receive buffer.
 *
 * The default is to let the operating system pick a value. Applications that
 * perform multiple downloads in parallel may need to use smaller receive
 * buffers to avoid exhausting the OS resources dedicated to TCP buffers.
 */
struct MaximumCurlSocketRecvSizeOption {
  using Type = std::size_t;
};

/**
 * Control the maximum socket send buffer.
 *
 * The default is to let the operating system pick a value, this is almost
 * always a good choice.
 */
struct MaximumCurlSocketSendSizeOption {
  using Type = std::size_t;
};

/**
 * Issue a new request to any Location header specified in a HTTP 3xx response.
 */
struct CurlFollowLocationOption {
  using Type = bool;
};

using CurlOptionList = ::google::cloud::OptionList<
    ConnectionPoolSizeOption, EnableCurlSslLockingOption,
    EnableCurlSigpipeHandlerOption, MaximumCurlSocketRecvSizeOption,
    MaximumCurlSocketSendSizeOption, CAPathOption, HttpVersionOption,
    CurlFollowLocationOption>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_OPTIONS_H
