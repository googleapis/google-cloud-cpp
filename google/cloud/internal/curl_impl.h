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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_IMPL_H

#include "google/cloud/internal/curl_handle.h"
#include "google/cloud/internal/curl_handle_factory.h"
#include "google/cloud/internal/curl_wrappers.h"
#include "google/cloud/internal/rest_request.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/types/span.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// libcurl will never pass a block larger than CURL_MAX_WRITE_SIZE to the
// [write callback](https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html).
// However, CurlImpl::Read() may not be given a buffer large enough to
// store it all, so we need a place to store the remainder.
class SpillBuffer {
 public:
  SpillBuffer() = default;

  std::size_t capacity() const { return buffer_.max_size(); }
  std::size_t size() const { return size_; }

  // Copy all the data from the source.
  std::size_t CopyFrom(absl::Span<char const> src);

  // Copy as much data as possible to the destination.
  std::size_t MoveTo(absl::Span<char> dst);

 private:
  // The logical content of the spill buffer is the size_ length prefix of
  // buffer_[start_ ... CURL_MAX_WRITE_SIZE-1] + buffer_[0 ... start_-1].
  std::array<char, CURL_MAX_WRITE_SIZE> buffer_;
  std::size_t start_ = 0;
  std::size_t size_ = 0;
};

// Encapsulates use of libcurl, managing all the necessary state for a
// request and its associated response.
class CurlImpl {
 public:
  enum class HttpMethod { kDelete, kGet, kPatch, kPost, kPut };

  CurlImpl(CurlHandle handle, std::shared_ptr<CurlHandleFactory> factory,
           Options const& options);
  ~CurlImpl();

  CurlImpl(CurlImpl const&) = delete;
  CurlImpl(CurlImpl&&) = default;
  CurlImpl& operator=(CurlImpl const&) = delete;
  CurlImpl& operator=(CurlImpl&&) = default;

  void SetHeader(std::string const& header);
  void SetHeader(std::pair<std::string, std::string> const& header);
  void SetHeaders(RestRequest const& request);

  std::string MakeEscapedString(std::string const& s);

  void SetUrl(std::string const& endpoint, RestRequest const& request,
              RestRequest::HttpParameters const& additional_parameters);
  std::string const& url() const { return url_; }

  std::string LastClientIpAddress() const;
  HttpStatusCode status_code() const { return http_code_; }
  std::multimap<std::string, std::string> const& headers() const {
    return received_headers_;
  }

  Status MakeRequest(HttpMethod method,
                     std::vector<absl::Span<char const>> request = {});

  bool HasUnreadData() const;
  StatusOr<std::size_t> Read(absl::Span<char> output);

  // Called from libcurl callbacks for received data.
  std::size_t WriteCallback(absl::Span<char> response);
  std::size_t HeaderCallback(absl::Span<char> response);

 private:
  Status MakeRequestImpl();
  StatusOr<std::size_t> ReadImpl(absl::Span<char> output);

  // Cleanup the CURL handles, leaving them ready for reuse.
  void CleanupHandles();
  // Perform at least part of the request.
  StatusOr<int> PerformWork();
  // Loop on PerformWork until a condition is met.
  Status PerformWorkUntil(absl::FunctionRef<bool()> predicate);
  // Wait until the underlying data can perform work.
  Status WaitForHandles(int& repeats);

  Status OnTransferError(Status status);
  void OnTransferDone();

  std::shared_ptr<CurlHandleFactory> factory_;
  CurlHeaders request_headers_;
  CurlHandle handle_;
  CurlMulti multi_;

  bool logging_enabled_;
  bool follow_location_;
  CurlHandle::SocketOptions socket_options_;
  std::string user_agent_;
  std::string http_version_;
  std::chrono::seconds transfer_stall_timeout_;
  std::uint32_t transfer_stall_minimum_rate_;
  std::chrono::seconds download_stall_timeout_;
  std::uint32_t download_stall_minimum_rate_;

  CurlReceivedHeaders received_headers_;
  std::string url_;
  HttpStatusCode http_code_;

  // Explicitly closing the handle happens in two steps:
  // 1. CurlImpl notifies libcurl that the transfer is terminated by
  //    returning 0 from WriteCallback().
  // 2. Once that happens, CurlImpl needs to wait until libcurl stops
  //    using the handle, which happens via PerformWork().
  //
  // Closing also happens automatically when the transfer completes
  // successfully or when the connection is dropped due to some error.
  // In both cases PerformWork() sets the curl_closed_ flag to true.
  //
  // The closing_ flag is set when we enter step 1.
  bool closing_ = false;
  // The curl_closed_ flag is set when we enter step 2, or when the transfer
  // completes.
  bool curl_closed_ = false;

  // Track whether handle_ has been added to multi_ or not. The exact
  // lifecycle for the handle depends on the libcurl version, and using
  // this flag makes the code less elegant, but less prone to bugs.
  bool in_multi_ = false;
  bool paused_ = false;

  // Track when status and headers from the response are received.
  bool all_headers_received_ = false;

  // Track the unused portion of the output buffer provided to Read().
  absl::Span<char> avail_;

  // Store pending data between WriteCallback() calls.
  SpillBuffer spill_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_IMPL_H
