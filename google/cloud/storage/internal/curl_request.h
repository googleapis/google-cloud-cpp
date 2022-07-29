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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_REQUEST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_REQUEST_H

#include "google/cloud/storage/internal/const_buffer.h"
#include "google/cloud/storage/internal/curl_handle.h"
#include "google/cloud/storage/internal/curl_handle_factory.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/version.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
extern "C" size_t CurlRequestOnWriteData(char* ptr, size_t size, size_t nmemb,
                                         void* userdata);
extern "C" size_t CurlRequestOnHeaderData(char* contents, size_t size,
                                          size_t nitems, void* userdata);

class CurlRequest {
 public:
  CurlRequest() = default;
  ~CurlRequest();

  CurlRequest(CurlRequest&&) = default;
  CurlRequest& operator=(CurlRequest&&) = default;

  /**
   * Makes the prepared request.
   *
   * This function can be called multiple times on the same request.
   *
   * @return The response HTTP error code, the headers and an empty payload.
   */
  StatusOr<HttpResponse> MakeRequest(std::string const& payload) &&;

  /// @copydoc MakeRequest(std::string const&)
  StatusOr<HttpResponse> MakeUploadRequest(ConstBufferSequence payload) &&;

 private:
  /// Handle a libcurl error during the request.
  Status OnError(Status status);

  StatusOr<HttpResponse> MakeRequestImpl();

  friend class CurlRequestBuilder;
  friend size_t CurlRequestOnWriteData(char* ptr, size_t size, size_t nmemb,
                                       void* userdata);
  friend size_t CurlRequestOnHeaderData(char* contents, size_t size,
                                        size_t nitems, void* userdata);

  std::size_t OnWriteData(char* contents, std::size_t size, std::size_t nmemb);
  std::size_t OnHeaderData(char* contents, std::size_t size,
                           std::size_t nitems);

  std::string url_;
  rest_internal::CurlHeaders headers_ =
      rest_internal::CurlHeaders(nullptr, &curl_slist_free_all);
  std::string user_agent_;
  std::string http_version_;
  std::string response_payload_;
  rest_internal::CurlReceivedHeaders received_headers_;
  bool logging_enabled_ = false;
  CurlHandle::SocketOptions socket_options_;
  std::chrono::seconds transfer_stall_timeout_;
  std::uint32_t transfer_stall_minimum_rate_;
  CurlHandle handle_;
  std::shared_ptr<rest_internal::CurlHandleFactory> factory_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_REQUEST_H
