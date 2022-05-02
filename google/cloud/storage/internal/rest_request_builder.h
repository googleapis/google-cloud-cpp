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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_REST_REQUEST_BUILDER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_REST_REQUEST_BUILDER_H

#include "google/cloud/storage/internal/complex_option.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/storage/well_known_headers.h"
#include "google/cloud/storage/well_known_parameters.h"
#include "google/cloud/internal/rest_request.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
/**
 * Implements a storage request Option aware Builder pattern wrapper around
 * google::cloud::rest_internal::RestRequest.
 */
class RestRequestBuilder {
 public:
  explicit RestRequestBuilder(std::string path);

  /**
   * Creates a RestRequest from the builder.
   *
   * This function invalidates the builder. The application should not use this
   * builder once this function is called.
   */
  google::cloud::rest_internal::RestRequest BuildRequest() &&;

  /// Adds one of the well-known parameters as a query parameter.
  template <typename P>
  RestRequestBuilder& AddOption(WellKnownParameter<P, std::string> const& p) {
    if (p.has_value()) {
      request_.AddQueryParameter(p.parameter_name(), p.value());
    }
    return *this;
  }

  /// Adds one of the well-known parameters as a query parameter.
  template <typename P>
  RestRequestBuilder& AddOption(WellKnownParameter<P, std::int64_t> const& p) {
    if (p.has_value()) {
      request_.AddQueryParameter(p.parameter_name(), std::to_string(p.value()));
    }
    return *this;
  }

  /// Adds one of the well-known parameters as a query parameter.
  template <typename P>
  RestRequestBuilder& AddOption(WellKnownParameter<P, bool> const& p) {
    if (!p.has_value()) {
      return *this;
    }
    request_.AddQueryParameter(p.parameter_name(),
                               p.value() ? "true" : "false");
    return *this;
  }

  /// Adds one of the well-known headers to the request.
  template <typename P>
  RestRequestBuilder& AddOption(WellKnownHeader<P, std::string> const& p) {
    if (p.has_value()) {
      request_.AddHeader(p.header_name(), p.value());
    }
    return *this;
  }

  /// Adds one of the well-known headers to the request.
  template <typename P, typename V,
            typename Enabled = typename std::enable_if<
                std::is_arithmetic<V>::value, void>::type>
  RestRequestBuilder& AddOption(WellKnownHeader<P, V> const& p) {
    if (p.has_value()) {
      request_.AddHeader(p.header_name(), p.value());
    }
    return *this;
  }

  /// Adds a custom header to the request.
  RestRequestBuilder& AddOption(CustomHeader const& p);

  /// Adds one of the well-known encryption header groups to the request.
  RestRequestBuilder& AddOption(EncryptionKey const& p);

  /// Adds one of the well-known encryption header groups to the request.
  RestRequestBuilder& AddOption(SourceEncryptionKey const& p);

  /**
   * Ignore complex options, these are managed explicitly in the requests that
   * use them.
   */
  template <typename Option, typename T>
  RestRequestBuilder& AddOption(ComplexOption<Option, T> const&) {
    // Ignore other options.
    return *this;
  }

  RestRequestBuilder& AddQueryParameter(std::string const& key,
                                        std::string const& value);

  RestRequestBuilder& AddHeader(std::string const& key,
                                std::string const& value);

 private:
  google::cloud::rest_internal::RestRequest request_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_REST_REQUEST_BUILDER_H
