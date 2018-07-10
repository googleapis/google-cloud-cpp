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

#include "google/cloud/storage/internal/curl_request.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
class CurlRequestBuilder {
 public:
  using RequestType = CurlRequest;

  explicit CurlRequestBuilder(std::string base_url);

  CurlRequest BuildRequest(std::string payload);

  /// Add one of the well-known parameters as a query parameter
  template <typename P>
  CurlRequestBuilder& AddWellKnownParameter(
      WellKnownParameter<P, std::string> const& p) {
    if (p.has_value()) {
      AddQueryParameter(p.parameter_name(), p.value());
    }
    return *this;
  }

  /// Add one of the well-known parameters as a query parameter
  template <typename P>
  CurlRequestBuilder& AddWellKnownParameter(
      WellKnownParameter<P, std::int64_t> const& p) {
    if (p.has_value()) {
      AddQueryParameter(p.parameter_name(), std::to_string(p.value()));
    }
    return *this;
  }

  /// Add a prefix to the user-agent string.
  CurlRequestBuilder& AddUserAgentPrefix(std::string const& prefix);

  /// Add request headers.
  CurlRequestBuilder& AddHeader(std::string const& header);

  /// Add a parameter for a POST request.
  CurlRequestBuilder& AddQueryParameter(std::string const& key,
                                        std::string const& value);

  /// Change the http method used for this request
  CurlRequestBuilder& SetMethod(std::string const& method);

  CurlRequestBuilder& SetDebugLogging(bool enabled);

  /// Get the user-agent suffix.
  std::string UserAgentSuffix() const;

  /// URL-escape a string.
  CurlString MakeEscapedString(std::string const& s) {
    return handle_.MakeEscapedString(s);
  }

 private:
  CurlHandle handle_;
  CurlHeaders headers_;

  std::string url_;
  char const* query_parameter_separator_;

  std::string user_agent_prefix_;

  bool logging_enabled_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_REQUEST_BUILDER_H_
