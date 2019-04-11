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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_DOWNLOAD_REQUEST_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_DOWNLOAD_REQUEST_H_

#include "google/cloud/log.h"
#include "google/cloud/storage/internal/curl_request.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/version.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Makes streaming download requests using libcurl.
 *
 * This class manages the resources and workflow to make requests where the
 * payload is streamed, and the total size is not known. Under the hood this
 * uses chunked transfer encoding.
 *
 * @see `CurlRequest` for simpler transfers where the size of the payload is
 *     known and relatively small.
 */
class CurlDownloadRequest {
 public:
  explicit CurlDownloadRequest(std::size_t initial_buffer_size);

  ~CurlDownloadRequest() {
    if (!factory_) {
      return;
    }
    factory_->CleanupHandle(std::move(handle_.handle_));
    factory_->CleanupMultiHandle(std::move(multi_));
  }

  CurlDownloadRequest(CurlDownloadRequest&& rhs) noexcept(false)
      : url_(std::move(rhs.url_)),
        headers_(std::move(rhs.headers_)),
        payload_(std::move(rhs.payload_)),
        user_agent_(std::move(rhs.user_agent_)),
        logging_enabled_(rhs.logging_enabled_),
        handle_(std::move(rhs.handle_)),
        multi_(std::move(rhs.multi_)),
        factory_(std::move(rhs.factory_)),
        closing_(rhs.closing_),
        curl_closed_(rhs.curl_closed_),
        initial_buffer_size_(rhs.initial_buffer_size_) {
    ResetOptions();
  }

  CurlDownloadRequest& operator=(CurlDownloadRequest&& rhs) noexcept {
    url_ = std::move(rhs.url_);
    headers_ = std::move(rhs.headers_);
    payload_ = std::move(rhs.payload_);
    user_agent_ = std::move(rhs.user_agent_);
    logging_enabled_ = rhs.logging_enabled_;
    handle_ = std::move(rhs.handle_);
    multi_ = std::move(rhs.multi_);
    factory_ = std::move(rhs.factory_);
    closing_ = rhs.closing_;
    curl_closed_ = rhs.curl_closed_;
    initial_buffer_size_ = rhs.initial_buffer_size_;
    ResetOptions();
    return *this;
  }

  bool IsOpen() const { return !curl_closed_; }
  StatusOr<HttpResponse> Close();

  /**
   * Waits for additional data or the end of the transfer.
   *
   * This operation blocks until `initial_buffer_size` data has been received or
   * the transfer is completed.
   *
   * @param buffer the location to return the new data. Note that the contents
   *     of this parameter are completely replaced with the new data.
   * @returns 100-Continue if the transfer is not yet completed.
   */
  StatusOr<HttpResponse> GetMore(std::string& buffer);

 private:
  friend class CurlRequestBuilder;
  /// Set the underlying CurlHandle options on a new CurlDownloadRequest.
  void SetOptions();

  /// Reset the underlying CurlHandle options after a move operation.
  void ResetOptions();

  /// Called by libcurl to show that more data is available in the download.
  std::size_t WriteCallback(void* ptr, std::size_t size, std::size_t nmemb);

  /// Wait until a condition is met.
  template <typename Predicate>
  Status Wait(Predicate&& predicate) {
    int repeats = 0;
    // We can assert that the current thread is the leader, because the
    // predicate is satisfied, and the condition variable exited. Therefore,
    // this thread must run the I/O event loop.
    while (!predicate()) {
      handle_.FlushDebug(__func__);
      GCP_LOG(DEBUG) << __func__ << "() predicate is false"
                     << ", curl.size=" << buffer_.size();
      auto running_handles = PerformWork();
      if (!running_handles.ok()) {
        return std::move(running_handles).status();
      }
      // Only wait if there are CURL handles with pending work *and* the
      // predicate is not satisfied. Note that if the predicate is ill-defined
      // it might continue to be unsatisfied even though the handles have
      // completed their work.
      if (*running_handles == 0 || predicate()) {
        return Status();
      }
      auto status = WaitForHandles(repeats);
      if (!status.ok()) {
        return status;
      }
    }
    return Status();
  }

  /// Use libcurl to perform at least part of the transfer.
  StatusOr<int> PerformWork();

  /// Use libcurl to wait until the underlying data can perform work.
  Status WaitForHandles(int& repeats);

  /// Simplify handling of errors in the curl_multi_* API.
  Status AsStatus(CURLMcode result, char const* where);

  std::string url_;
  CurlHeaders headers_;
  std::string payload_;
  std::string user_agent_;
  CurlReceivedHeaders received_headers_;
  bool logging_enabled_;
  CurlHandle handle_;
  CurlMulti multi_;
  std::shared_ptr<CurlHandleFactory> factory_;

  std::string buffer_;
  // Closing the handle happens in two steps.
  // 1. First the application (or higher-level class), calls Close(). This class
  //    needs to notify libcurl that the transfer is terminated by returning 0
  //    from the callback.
  // 2. Once that callback returns 0, this class needs to know
  //
  // The closing_ flag is set when we enter step 1.
  bool closing_;
  // The curl_closed_ flag is set when we enter step 2, or when the transfer
  // completes.
  bool curl_closed_;

  std::size_t initial_buffer_size_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_DOWNLOAD_REQUEST_H_
