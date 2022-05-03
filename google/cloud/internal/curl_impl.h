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
#include <utility>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

extern "C" std::size_t RestCurlRequestWrite(char* ptr, size_t size,
                                            size_t nmemb, void* userdata);
extern "C" std::size_t RestCurlRequestHeader(char* contents, std::size_t size,
                                             std::size_t nitems,
                                             void* userdata);

// This class encapsulates use of libcurl and manages all the necessary state
// of a request and its associated response.
class CurlImpl {
 public:
  enum class HttpMethod { kDelete, kGet, kPatch, kPost, kPut };
  CurlImpl(CurlHandle handle, std::shared_ptr<CurlHandleFactory> factory,
           Options options);
  ~CurlImpl();

  CurlImpl(CurlImpl const&) = delete;
  CurlImpl(CurlImpl&&) = default;
  CurlImpl& operator=(CurlImpl const&) = delete;
  CurlImpl& operator=(CurlImpl&&) = default;

  void SetHeader(std::string const& header);
  void SetHeader(std::pair<std::string, std::string> const& header);
  void SetHeaders(RestRequest const& request);

  void SetUrl(std::string const& endpoint, RestRequest const& request,
              RestRequest::HttpParameters const& additional_parameters);

  inline std::string LastClientIpAddress() const {
    return factory_->LastClientIpAddress();
  }
  HttpStatusCode status_code() const {
    return static_cast<HttpStatusCode>(http_code_);
  }
  inline std::multimap<std::string, std::string> const& headers() const {
    return received_headers_;
  }
  std::string const& url() const { return url_; }
  std::string MakeEscapedString(std::string const& payload) {
    return handle_.MakeEscapedString(payload).get();
  }
  Status MakeRequest(CurlImpl::HttpMethod method,
                     std::vector<absl::Span<char const>> payload = {});
  StatusOr<std::size_t> Read(absl::Span<char> output);

 private:
  friend std::size_t RestCurlRequestWrite(char* ptr, size_t size, size_t nmemb,
                                          void* userdata);
  friend std::size_t RestCurlRequestHeader(char* contents, std::size_t size,
                                           std::size_t nitems, void* userdata);

  // Called by libcurl to show that more data is available in the request.
  std::size_t WriteCallback(void* ptr, std::size_t size, std::size_t nmemb);
  std::size_t HeaderCallback(char* contents, std::size_t size,
                             std::size_t nitems);

  std::size_t WriteToUserBuffer(void* ptr, std::size_t size, std::size_t nmemb);
  std::size_t WriteAllBytesToSpillBuffer(void* ptr, std::size_t size,
                                         std::size_t nmemb);

  void ApplyOptions(Options const& options);
  StatusOr<std::size_t> ReadImpl(absl::Span<char> output);
  Status MakeRequestImpl();

  // Cleanup the CURL handles, leaving them ready for reuse.
  void CleanupHandles();
  // Copy any available data from the spill buffer to `buffer_`
  std::size_t DrainSpillBuffer();
  // Use libcurl to perform at least part of the request.
  StatusOr<int> PerformWork();
  // Use libcurl to wait until the underlying data can perform work.
  Status WaitForHandles(int& repeats);
  // Loop on PerformWork until a condition is met.
  Status PerformWorkUntil(absl::FunctionRef<bool()> predicate);
  // Simplify handling of errors in the curl_multi_* API.
  static Status AsStatus(CURLMcode result, char const* where);
  Status OnTransferError(Status status);
  void OnTransferDone();

  std::shared_ptr<CurlHandleFactory> factory_;
  CurlHeaders request_headers_;
  CurlReceivedHeaders received_headers_;
  std::string url_;
  std::string user_agent_;
  bool logging_enabled_ = false;
  CurlHandle::SocketOptions socket_options_;
  std::chrono::seconds transfer_stall_timeout_;
  std::chrono::seconds download_stall_timeout_;
  std::string http_version_;
  std::int32_t http_code_;

  // Explicitly closing the handle happens in two steps.
  // 1. This class needs to notify libcurl that the transfer is terminated by
  //    returning 0 from the callback.
  // 2. Once that callback returns 0, this class needs to wait until libcurl
  //    stops using the handle, which happens via PerformWork().
  //
  // Closing also happens automatically when the transfer completes successfully
  // or when the connection is dropped due to some error. In both cases
  // PerformWork() sets the curl_closed_ flags to true.
  //
  // The closing_ flag is set when we enter step 1.
  bool closing_ = false;
  // The curl_closed_ flag is set when we enter step 2, or when the transfer
  // completes.
  bool curl_closed_ = false;

  CurlHandle handle_;
  CurlMulti multi_;
  // Track whether `handle_` has been added to `multi_` or not. The exact
  // lifecycle for the handle depends on the libcurl version, and using this
  // flag makes the code less elegant, but less prone to bugs.
  bool in_multi_ = false;
  bool paused_ = false;

  // Track when status and headers from the response are received.
  bool all_headers_received_ = false;

  // Track the usage of the buffer provided to Read.
  absl::Span<char> buffer_;

  // libcurl(1) will never pass a block larger than CURL_MAX_WRITE_SIZE to
  // `WriteCallback()`:
  //     https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
  // However, the callback *must* save all the bytes.  If the callback reports
  // that not all bytes are processed the download is aborted. The application
  // may not provide a large enough buffer in the call to `Read()`, so we need a
  // place to store the additional bytes.
  std::array<char, CURL_MAX_WRITE_SIZE> spill_;
  std::size_t spill_offset_ = 0;

  Options options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_IMPL_H
