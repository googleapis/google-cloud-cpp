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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OPTIONS_H

#include "google/cloud/storage/idempotency_policy.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/credentials.h"
#include "google/cloud/options.h"
#include <chrono>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage_experimental {
inline namespace STORAGE_CLIENT_NS {
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
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage_experimental

namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/// This is only intended for testing against staging or development versions
/// of the service. It is not for public use.
struct TargetApiVersionOption {
  using Type = std::string;
};

/// This is only intended for testing. It is not for public use.
struct CAPathOption {
  using Type = std::string;
};

}  // namespace internal

/// Configure the REST endpoint for the GCS client library.
struct RestEndpointOption {
  using Type = std::string;
};

/// Configure the IAM endpoint for the GCS client library.
struct IamEndpointOption {
  using Type = std::string;
};

/// Configure oauth2::Credentials for the GCS client library.
struct Oauth2CredentialsOption {
  using Type = std::shared_ptr<oauth2::Credentials>;
};

/// Set the Google Cloud Platform project id.
struct ProjectIdOption {
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
 * @warning The library may create more connections than this option configures,
 * for example if your application requests many simultaneous downloads.
 */
struct ConnectionPoolSizeOption {
  using Type = std::size_t;
};

/**
 * Control the formatted I/O download buffer.
 *
 * When using formatted I/O operations (typically `operator>>(std::istream&...)`
 * this option controls the size of the in-memory buffer kept to satisfy any I/O
 * requests.
 *
 * Applications seeking optional performance for downloads should avoid
 * formatted I/O, and prefer using `std::istream::read()`. This option has no
 * effect in that case.
 */
struct DownloadBufferSizeOption {
  using Type = std::size_t;
};

/**
 * Control the formatted I/O upload buffer.
 *
 * When using formatted I/O operations (typically `operator<<(std::istream&...)`
 * this option controls the size of the in-memory buffer kept before a chunk is
 * uploaded. Note that GCS only accepts chunks in multiples of 256KiB, so this
 * option is always rounded up to the next such multiple.
 *
 * Applications seeking optional performance for downloads should avoid
 * formatted I/O, and prefer using `std::istream::write()`. This option has no
 * effect in that case.
 */
struct UploadBufferSizeOption {
  using Type = std::size_t;
};

/**
 * Defines the threshold to switch from simple to resumable uploads for files.
 *
 * When uploading small files the faster approach is to use a simple upload. For
 * very large files this is not feasible, as the whole file may not fit in
 * memory (we are ignoring memory mapped files in this discussion). The library
 * automatically switches to resumable upload for files larger than this
 * threshold.
 */
struct MaximumSimpleUploadSizeOption {
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
 * Sets the "stall" timeout.
 *
 * If the download "stalls", i.e., no bytes are received for a significant
 * period, it may be better to restart the download as this may indicate a
 * network glitch.
 */
struct DownloadStallTimeoutOption {
  using Type = std::chrono::seconds;
};

/// Set the retry policy for a GCS client.
struct RetryPolicyOption {
  using Type = std::shared_ptr<RetryPolicy>;
};

/// Set the backoff policy for a GCS client.
struct BackoffPolicyOption {
  using Type = std::shared_ptr<BackoffPolicy>;
};

/// Set the idempotency policy for a GCS client.
struct IdempotencyPolicyOption {
  using Type = std::shared_ptr<IdempotencyPolicy>;
};

/// The complete list of options accepted by `storage::Client`.
using ClientOptionList = ::google::cloud::OptionList<
    RestEndpointOption, IamEndpointOption, Oauth2CredentialsOption,
    ProjectIdOption, ProjectIdOption, ConnectionPoolSizeOption,
    DownloadBufferSizeOption, UploadBufferSizeOption,
    EnableCurlSslLockingOption, EnableCurlSigpipeHandlerOption,
    MaximumCurlSocketRecvSizeOption, MaximumCurlSocketSendSizeOption,
    DownloadStallTimeoutOption, RetryPolicyOption, BackoffPolicyOption,
    IdempotencyPolicyOption, CARootsFilePathOption,
    storage_experimental::HttpVersionOption>;

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OPTIONS_H
