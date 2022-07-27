// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_HANDLE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_HANDLE_H

#include "google/cloud/internal/curl_wrappers.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/functional/function_ref.h"
#include <curl/curl.h>
#include <string>
#include <type_traits>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class CurlHandleFactory;

/**
 * Wraps CURL* handles in a safer C++ interface.
 *
 * This is a fairly straightforward wrapper around the CURL* handle. It provides
 * nicer C++-style API for the curl_*() functions, and some helpers to ease
 * the use of the API.
 */
class CurlHandle {
 public:
  static CurlHandle MakeFromPool(CurlHandleFactory& factory);
  static void ReturnToPool(CurlHandleFactory& factory, CurlHandle h);
  static void DiscardFromPool(CurlHandleFactory& factory, CurlHandle h);

  explicit CurlHandle();
  ~CurlHandle();

  // This class holds unique ptrs, disable copying.
  CurlHandle(CurlHandle const&) = delete;
  CurlHandle& operator=(CurlHandle const&) = delete;

  // Allow moves, some care is needed to guarantee the pointers passed to the
  // libcurl C callbacks (`debug`, and `socket`) are stable.
  // * For the `debug` callback (used rarely), we use a `std::shared_ptr<>`.
  // * For the `socket` callback, the only classes that use it are
  //   `CurlDownloadRequest` and `CurlRequest`.  These classes guarantee the
  //   object is not move-constructed-from or move-assigned-from once the
  //   callback is set up.
  CurlHandle(CurlHandle&&) = default;
  CurlHandle& operator=(CurlHandle&&) = default;

  /// Set the callback to initialize each socket.
  struct SocketOptions {
    std::size_t recv_buffer_size_ = 0;
    std::size_t send_buffer_size_ = 0;
  };

  void SetSocketCallback(SocketOptions const& options);

  /// URL-escapes a string.
  CurlString MakeEscapedString(std::string const& s) {
    return CurlString(
        curl_easy_escape(handle_.get(), s.data(), static_cast<int>(s.length())),
        &curl_free);
  }

  template <typename T>
  Status SetOption(CURLoption option, T&& param) {
    auto e = curl_easy_setopt(handle_.get(), option, std::forward<T>(param));
    return AsStatus(e, __func__);
  }

  Status SetOption(CURLoption option, std::nullptr_t) {
    auto e = curl_easy_setopt(handle_.get(), option, nullptr);
    return AsStatus(e, __func__);
  }

  /**
   * Sets an option that may fail.
   *
   * The common case to use this is setting an option that sometimes is disabled
   * in libcurl at compile-time. For example, libcurl can be compiled without
   * HTTP/2 support, requesting HTTP/2 results in a (harmless) error.
   */
  template <typename T>
  void SetOptionUnchecked(CURLoption option, T&& param) {
    (void)curl_easy_setopt(handle_.get(), option, std::forward<T>(param));
  }

  Status EasyPerform() {
    auto e = curl_easy_perform(handle_.get());
    return AsStatus(e, __func__);
  }

  /// Gets the HTTP response code.
  std::int32_t GetResponseCode();

  /**
   * Gets a string identifying the peer.
   *
   * It always returns a non-empty string, even if there is an error. The
   * contents of the string if there was an error are otherwise unspecified.
   */
  std::string GetPeer();

  Status EasyPause(int bitmask) {
    auto e = curl_easy_pause(handle_.get(), bitmask);
    return AsStatus(e, __func__);
  }

  void EnableLogging(bool enabled);

  /// Flushes any debug data using GCP_LOG().
  void FlushDebug(char const* where);

  /// Convert a CURLE_* error code to a google::cloud::Status().
  static Status AsStatus(CURLcode e, char const* where);

  struct DebugInfo {
    std::string buffer;
    std::uint64_t recv_zero_count = 0;
    std::uint64_t recv_count = 0;
    std::uint64_t send_zero_count = 0;
    std::uint64_t send_count = 0;
  };

 private:
  explicit CurlHandle(CurlPtr ptr) : handle_(std::move(ptr)) {}

  friend class CurlImpl;

  CurlPtr handle_;
  std::shared_ptr<DebugInfo> debug_info_;
  SocketOptions socket_options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_HANDLE_H
