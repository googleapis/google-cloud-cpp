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

#ifndef GOOGLE_CLOUD_CPP_STORAGE_CLIENT_INTERNAL_CURL_REQUEST_H_
#define GOOGLE_CLOUD_CPP_STORAGE_CLIENT_INTERNAL_CURL_REQUEST_H_

#include "storage/client/internal/curl_wrappers.h"
#include "storage/client/internal/nljson.h"
#include <curl/curl.h>

namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/// Hold a character string created by CURL use correct deleter.
using CurlString = std::unique_ptr<char, decltype(&curl_free)>;

class CurlRequest {
 public:
  explicit CurlRequest(std::string base_url);
  ~CurlRequest();

  /// Add request headers.
  void AddHeader(std::string const& key, std::string const& value);
  void AddHeader(std::string const& header);

  /// Add a parameter for a POST request.
  void AddQueryParameter(std::string const& key, std::string const& value);

  /// URL-escape a string.
  CurlString MakeEscapedString(std::string const& s) {
    return CurlString(
        curl_easy_escape(curl_, s.data(), static_cast<int>(s.length())),
        &curl_free);
  }

  /**
   * Make a request with the given payload.
   *
   * @param payload The contents of the request.
   */
  void PrepareRequest(std::string payload);

  /**
   * Make a request with the given payload.
   *
   * @param payload The JSON object to send as part of the request.
   */
  void PrepareRequest(nl::json payload);

  /// Make the prepared request and return the responsee.
  std::string MakeRequest();

  CurlRequest(CurlRequest const&) = delete;
  CurlRequest& operator=(CurlRequest const&) = delete;
  CurlRequest(CurlRequest&&) = default;
  CurlRequest& operator=(CurlRequest&&) = default;

 private:
  std::string url_;
  char const* query_parameter_separator_;
  CURL* curl_;
  curl_slist* headers_;
  CurlBuffer buffer_;
  std::string payload_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage

#endif  // GOOGLE_CLOUD_CPP_STORAGE_CLIENT_INTERNAL_CURL_REQUEST_H_
