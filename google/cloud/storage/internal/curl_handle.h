// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_HANDLE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_HANDLE_H

#include "google/cloud/storage/client_options.h"
#include "google/cloud/storage/internal/curl_wrappers.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/status_or.h"
#include <curl/curl.h>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Wraps CURL* handles in a safer C++ interface.
 *
 * This is a fairly straightforward wrapper around the CURL* handle. It provides
 * nicer C++-style API for the curl_*() functions, and some helpers to ease
 * the use of the API.
 */
class CurlHandle {
 public:
  explicit CurlHandle();
  ~CurlHandle();

  // This class holds unique ptrs, disable copying.
  CurlHandle(CurlHandle const&) = delete;
  CurlHandle& operator=(CurlHandle const&) = delete;

  // Allow moves, they immediately disable callbacks.
  CurlHandle(CurlHandle&&) = default;
  CurlHandle& operator=(CurlHandle&&) = default;

  /// Set the callback to initialize each socket.
  struct SocketOptions {
    std::size_t recv_buffer_size_ = 0;
    std::size_t send_buffer_size_ = 0;
  };

  void SetSocketCallback(SocketOptions const& options);

  /// Reset the socket callback.
  void ResetSocketCallback();

  /// URL-escapes a string.
  CurlString MakeEscapedString(std::string const& s) {
    return CurlString(
        curl_easy_escape(handle_.get(), s.data(), static_cast<int>(s.length())),
        &curl_free);
  }

  template <typename T>
  void SetOption(CURLoption option, T&& param) {
    auto e = curl_easy_setopt(handle_.get(), option, std::forward<T>(param));
    if (e == CURLE_OK) {
      return;
    }
    ThrowSetOptionError(e, option, std::forward<T>(param));
  }

  Status EasyPerform() {
    auto e = curl_easy_perform(handle_.get());
    return AsStatus(e, __func__);
  }

  StatusOr<long> GetResponseCode() {  // NOLINT(google-runtime-int)
    long code;                        // NOLINT(google-runtime-int)
    auto e = curl_easy_getinfo(handle_.get(), CURLINFO_RESPONSE_CODE, &code);
    if (e == CURLE_OK) {
      return code;
    }
    return AsStatus(e, __func__);
  }

  Status EasyPause(int bitmask) {
    auto e = curl_easy_pause(handle_.get(), bitmask);
    return AsStatus(e, __func__);
  }

  void EnableLogging(bool enabled);

  /// Flushes any debug data using GCP_LOG().
  void FlushDebug(char const* where);

  /// Convert a CURLE_* error code to a google::cloud::Status().
  static Status AsStatus(CURLcode e, char const* where);

 private:
  explicit CurlHandle(CurlPtr ptr) : handle_(std::move(ptr)) {}

  friend class CurlDownloadRequest;
  friend class CurlRequestBuilder;
  friend class CurlHandleFactory;

  [[noreturn]] static void ThrowSetOptionError(CURLcode e, CURLoption opt,
                                               std::intmax_t param);
  [[noreturn]] static void ThrowSetOptionError(CURLcode e, CURLoption opt,
                                               char const* param);
  [[noreturn]] static void ThrowSetOptionError(CURLcode e, CURLoption opt,
                                               void* param);
  template <typename T>
  [[noreturn]] static void ThrowSetOptionError(CURLcode e, CURLoption opt, T) {
    std::string param = "complex-type=<";
    param += typeid(T).name();
    param += ">";
    ThrowSetOptionError(e, opt, param.c_str());
  }

  CurlPtr handle_;
  std::string debug_buffer_;
  SocketOptions socket_options_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_HANDLE_H
