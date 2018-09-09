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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_REQUEST_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_REQUEST_H_

#include "google/cloud/storage/internal/curl_handle.h"
#include "google/cloud/storage/internal/curl_handle_factory.h"
#include "google/cloud/storage/internal/http_response.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Makes RPC-like requests using CURL.
 *
 * The Google Cloud Storage Client library using libcurl to make http requests,
 * this class manages the resources and workflow to make a simple RPC-like
 * request.
 */
class CurlRequest {
 public:
  CurlRequest();

  ~CurlRequest() {
    if (not factory_) {
      return;
    }
    factory_->CleanupHandle(std::move(handle_.handle_));
  }

  CurlRequest(CurlRequest&& rhs) noexcept(false)
      : url_(std::move(rhs.url_)),
        headers_(std::move(rhs.headers_)),
        user_agent_(std::move(rhs.user_agent_)),
        response_payload_(std::move(rhs.response_payload_)),
        received_headers_(std::move(rhs.received_headers_)),
        logging_enabled_(rhs.logging_enabled_),
        handle_(std::move(rhs.handle_)),
        factory_(std::move(rhs.factory_)) {
    ResetOptions();
  }

  CurlRequest& operator=(CurlRequest&& rhs) noexcept(false) {
    url_ = std::move(rhs.url_);
    headers_ = std::move(rhs.headers_);
    user_agent_ = std::move(rhs.user_agent_);
    response_payload_ = std::move(rhs.response_payload_);
    received_headers_ = std::move(rhs.received_headers_);
    logging_enabled_ = rhs.logging_enabled_;
    handle_ = std::move(rhs.handle_);
    factory_ = std::move(rhs.factory_);

    ResetOptions();
    return *this;
  }

  /**
   * Makes the prepared request.
   *
   * This function can be called multiple times on the same request.
   *
   * @return The response HTTP error code and the response payload.
   *
   * @throw std::runtime_error if the request cannot be made at all.
   */
  HttpResponse MakeRequest(std::string const& payload);

 private:
  friend class CurlRequestBuilder;
  void ResetOptions();

  std::string url_;
  CurlHeaders headers_;
  std::string user_agent_;
  std::string response_payload_;
  CurlReceivedHeaders received_headers_;
  bool logging_enabled_;
  CurlHandle handle_;
  std::shared_ptr<CurlHandleFactory> factory_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_REQUEST_H_
