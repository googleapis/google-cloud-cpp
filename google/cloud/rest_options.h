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
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace experimental {

/**
 * A copyable handle for cooperatively cancelling synchronous REST operations.
 *
 * Copies share state. Cancel() is irreversible, idempotent, and safe to call
 * concurrently with IsCancelled() on this handle or a copy. Do not assign or
 * destroy the same handle concurrently with these calls. This is not a signal
 * handler API: invoke Cancel() from an ordinary application thread.
 *
 * Operations retain their own copy, so the application handle need not outlive
 * them. Construct a new handle for each independently cancellable operation.
 */
class RestCancellation {
 public:
  /// Creates independent, initially uncancelled state.
  RestCancellation() : cancelled_(std::make_shared<std::atomic<bool>>(false)) {}

  /// Requests cancellation of every operation sharing this handle's state.
  void Cancel() const { cancelled_->store(true, std::memory_order_relaxed); }

  /// Returns whether cancellation has been requested.
  bool IsCancelled() const {
    return cancelled_->load(std::memory_order_relaxed);
  }

 private:
  std::shared_ptr<std::atomic<bool>> cancelled_;
};

/**
 * Associates a RestCancellation with synchronous REST requests.
 *
 * Pass this in per-call Options to storage::Client. ReadObject() and
 * WriteObject() retain the option for the entire stream, including retries and
 * Close(). Setting it on a client deliberately shares cancellation across all
 * calls inheriting that option. Other calls on the client are unaffected.
 *
 * Cancellation is reported as StatusCode::kCancelled through the existing
 * operation result or stream error. It prevents further retries and upload
 * finalization during cleanup. It does not roll back server-side effects: when
 * cancellation races with completion, success or cancellation may be returned,
 * and the server may have committed the operation even if cancellation wins.
 *
 * Cancellable libcurl waits check state at intervals of at most 50ms; thread
 * scheduling, synchronous DNS resolvers, and application callbacks can delay
 * observation. Credential acquisition already in progress is not interrupted.
 * This option does not cancel gRPC operations. Buffered stream I/O may complete
 * before the next transport call observes cancellation.
 * Setting this option disables Storage read hedging for the stream, even before
 * cancellation is requested, so occupied hedging workers cannot delay it.
 *
 * @ingroup rest-options
 */
struct RestCancellationOption {
  using Type = RestCancellation;
};

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
                                ServerTimeoutOption, UserIpOption, Interface,
                                experimental::SslCtxCallbackOption,
                                experimental::RestCancellationOption>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_REST_OPTIONS_H
