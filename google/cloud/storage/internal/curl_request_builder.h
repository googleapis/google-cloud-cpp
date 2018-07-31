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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_REQUEST_BUILDER_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_REQUEST_BUILDER_H_

#include "google/cloud/storage/internal/curl_download_request.h"
#include "google/cloud/storage/internal/curl_request.h"
#include "google/cloud/storage/internal/curl_upload_request.h"
#include "google/cloud/storage/well_known_headers.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Implement the Builder pattern for CurlRequest, and CurlUploadRequest.
 */
class CurlRequestBuilder {
 public:
  using RequestType = CurlRequest;
  using UploadType = CurlUploadRequest;

  explicit CurlRequestBuilder(std::string base_url);

  /**
   * Create a http request with the given payload.
   *
   * This function invalidates the builder. The application should not use this
   * builder once this function is called.
   */
  CurlRequest BuildRequest(std::string payload);

  /**
   * Create a http request where the payload is provided dynamically.
   *
   * This function invalidates the builder. The application should not use this
   * builder once this function is called.
   */
  CurlUploadRequest BuildUpload();

  /**
   * Create a non-blocking http request.
   *
   * This function invalidates the builder. The application should not use this
   * builder once this function is called.
   */
  CurlDownloadRequest BuildDownloadRequest(std::string payload);

  /// Add one of the well-known parameters as a query parameter
  template <typename P>
  CurlRequestBuilder& AddOption(WellKnownParameter<P, std::string> const& p) {
    if (p.has_value()) {
      AddQueryParameter(p.parameter_name(), p.value());
    }
    return *this;
  }

  /// Add one of the well-known parameters as a query parameter
  template <typename P>
  CurlRequestBuilder& AddOption(WellKnownParameter<P, std::int64_t> const& p) {
    if (p.has_value()) {
      AddQueryParameter(p.parameter_name(), std::to_string(p.value()));
    }
    return *this;
  }

  /// Add one of the well-known headers to the request.
  template <typename P>
  CurlRequestBuilder& AddOption(WellKnownHeader<P, std::string> const& p) {
    if (p.has_value()) {
      std::string header = p.header_name();
      header += ": ";
      header += p.value();
      AddHeader(header);
    }
    return *this;
  }

  /// Add a prefix to the user-agent string.
  CurlRequestBuilder& AddUserAgentPrefix(std::string const& prefix);

  /// Add request headers.
  CurlRequestBuilder& AddHeader(std::string const& header);

  /// Add a parameter for a request.
  CurlRequestBuilder& AddQueryParameter(std::string const& key,
                                        std::string const& value);

  /// Change the http method used for this request
  CurlRequestBuilder& SetMethod(std::string const& method);

  CurlRequestBuilder& SetDebugLogging(bool enabled);

  CurlRequestBuilder& SetInitialBufferSize(std::size_t size);

  /// Get the user-agent suffix.
  std::string UserAgentSuffix() const;

  /// URL-escape a string.
  CurlString MakeEscapedString(std::string const& s) {
    return handle_.MakeEscapedString(s);
  }

 private:
  void ValidateBuilderState(char const* where) const;

  CurlHandle handle_;
  CurlHeaders headers_;

  std::string url_;
  char const* query_parameter_separator_;

  std::string user_agent_prefix_;

  bool logging_enabled_;

  std::size_t initial_buffer_size_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_REQUEST_BUILDER_H_
