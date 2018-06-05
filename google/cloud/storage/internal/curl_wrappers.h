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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_WRAPPERS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_WRAPPERS_H_

#include "google/cloud/storage/version.h"
#include <curl/curl.h>
#include <map>

namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Receive the output from a libcurl request.
 */
class CurlBuffer {
 public:
  CurlBuffer() = default;

  /// Use this object to capture the payload of the @p curl handle.
  void Attach(CURL* curl);

  /// Return the contents and reset the contents.
  std::string contents() { return std::move(buffer_); }

  /// Add data to the buffer.
  void Append(char* data, std::size_t size);

 private:
  std::string buffer_;
};

class CurlHeaders {
 public:
  CurlHeaders() = default;

  /// Use this object to capture the headers of the @p curl handle.
  void Attach(CURL* curl);

  /// Return the contents and reset them.
  std::multimap<std::string, std::string> contents() {
    return std::move(contents_);
  }

  /// Add a new header line to the contents.
  void Append(char* data, std::size_t size);

 private:
  std::multimap<std::string, std::string> contents_;
};
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_WRAPPERS_H_
