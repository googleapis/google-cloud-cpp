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
#include <utility>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// This class encapsulates use of libcurl and manages all the necessary state
// of a request and its associated response.
class CurlImpl {
 public:
  enum class HttpMethod { kDelete, kGet, kPatch, kPost, kPut };

  CurlImpl(CurlImpl const&) = delete;
  CurlImpl(CurlImpl&&) = default;
  CurlImpl& operator=(CurlImpl const&) = delete;
  CurlImpl& operator=(CurlImpl&&) = default;

  void SetHeader(std::string const&) {}
  void SetHeader(std::pair<std::string, std::string> const&) {}
  void SetHeaders(RestRequest const&) {}
  void SetUrl(std::string const&, RestRequest const&,
              RestRequest::HttpParameters const&) {}

  std::string LastClientIpAddress() const { return last_ip_; }

  HttpStatusCode status_code() const { return http_code_; }
  std::multimap<std::string, std::string> const& headers() const {
    return headers_;
  }

  std::string const& url() const { return url_; }

  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  std::string MakeEscapedString(std::string const&) {
    // TODO(sdhart): implement this function
    return {};
  }

  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  Status MakeRequest(CurlImpl::HttpMethod,
                     std::vector<absl::Span<char const>> const&) {
    // TODO(sdhart): implement this function
    return {};
  }

  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  StatusOr<std::size_t> Read(absl::Span<char>) {
    // TODO(sdhart): implement this function
    return 0;
  }

 private:
  HttpStatusCode http_code_;
  std::string last_ip_;
  std::string url_;
  std::multimap<std::string, std::string> headers_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_IMPL_H
