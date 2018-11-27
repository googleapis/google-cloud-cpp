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

#include "google/cloud/storage/internal/complex_option.h"
#include "google/cloud/storage/internal/curl_download_request.h"
#include "google/cloud/storage/internal/curl_handle_factory.h"
#include "google/cloud/storage/internal/curl_request.h"
#include "google/cloud/storage/internal/curl_upload_request.h"
#include "google/cloud/storage/well_known_headers.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Implements the Builder pattern for CurlRequest, and CurlUploadRequest.
 */
class CurlRequestBuilder {
 public:
  using RequestType = CurlRequest;
  using UploadType = CurlUploadRequest;

  explicit CurlRequestBuilder(std::string base_url,
                              std::shared_ptr<CurlHandleFactory> factory);

  /**
   * Creates a http request with the given payload.
   *
   * This function invalidates the builder. The application should not use this
   * builder once this function is called.
   */
  CurlRequest BuildRequest();

  /**
   * Create a http request where the payload is provided dynamically.
   *
   * This function invalidates the builder. The application should not use this
   * builder once this function is called.
   */
  CurlUploadRequest BuildUpload();

  /**
   * Creates a non-blocking http request.
   *
   * This function invalidates the builder. The application should not use this
   * builder once this function is called.
   */
  CurlDownloadRequest BuildDownloadRequest(std::string payload);

  /// Adds one of the well-known parameters as a query parameter
  template <typename P>
  CurlRequestBuilder& AddOption(WellKnownParameter<P, std::string> const& p) {
    if (p.has_value()) {
      AddQueryParameter(p.parameter_name(), p.value());
    }
    return *this;
  }

  /// Adds one of the well-known parameters as a query parameter
  template <typename P>
  CurlRequestBuilder& AddOption(WellKnownParameter<P, std::int64_t> const& p) {
    if (p.has_value()) {
      AddQueryParameter(p.parameter_name(), std::to_string(p.value()));
    }
    return *this;
  }

  /// Adds one of the well-known parameters as a query parameter
  template <typename P>
  CurlRequestBuilder& AddOption(WellKnownParameter<P, bool> const& p) {
    if (not p.has_value()) {
      return *this;
    }
    AddQueryParameter(p.parameter_name(), p.value() ? "true" : "false");
    return *this;
  }

  /// Adds one of the well-known headers to the request.
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

  /// Adds a custom header to the request.
  CurlRequestBuilder& AddOption(CustomHeader const& p) {
    if (p.has_value()) {
      AddHeader(p.custom_header_name() + ": " + p.value());
    }
    return *this;
  }

  /// Adds one of the well-known encryption header groups to the request.
  CurlRequestBuilder& AddOption(EncryptionKey const& p) {
    if (p.has_value()) {
      AddHeader(std::string(p.prefix()) + "algorithm: " + p.value().algorithm);
      AddHeader(std::string(p.prefix()) + "key: " + p.value().key);
      AddHeader(std::string(p.prefix()) + "key-sha256: " + p.value().sha256);
    }
    return *this;
  }

  /// Adds one of the well-known encryption header groups to the request.
  CurlRequestBuilder& AddOption(SourceEncryptionKey const& p) {
    if (p.has_value()) {
      AddHeader(std::string(p.prefix()) + "Algorithm: " + p.value().algorithm);
      AddHeader(std::string(p.prefix()) + "Key: " + p.value().key);
      AddHeader(std::string(p.prefix()) + "Key-Sha256: " + p.value().sha256);
    }
    return *this;
  }

  /**
   * Ignore complex options, these are managed explicitly in the requests that
   * use them.
   */
  template <typename Option, typename T>
  CurlRequestBuilder& AddOption(ComplexOption<Option, T> const& p) {
    return *this;
  }

  /// Adds a prefix to the user-agent string.
  CurlRequestBuilder& AddUserAgentPrefix(std::string const& prefix);

  /// Adds request headers.
  CurlRequestBuilder& AddHeader(std::string const& header);

  /// Adds a parameter for a request.
  CurlRequestBuilder& AddQueryParameter(std::string const& key,
                                        std::string const& value);

  /// Changes the http method used for this request.
  CurlRequestBuilder& SetMethod(std::string const& method);

  /// Enables (or disables) debug logging.
  CurlRequestBuilder& SetDebugLogging(bool enabled);

  /// Sets the CURLSH* handle to share resources.
  CurlRequestBuilder& SetCurlShare(CURLSH* share);

  CurlRequestBuilder& SetInitialBufferSize(std::size_t size);

  /// Gets the user-agent suffix.
  std::string UserAgentSuffix() const;

  /// URL-escapes a string.
  CurlString MakeEscapedString(std::string const& s) {
    return handle_.MakeEscapedString(s);
  }

  /// Get the last local IP address from the factory.
  std::string LastClientIpAddress() const {
    return factory_->LastClientIpAddress();
  }

 private:
  void ValidateBuilderState(char const* where) const;

  std::shared_ptr<CurlHandleFactory> factory_;

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
