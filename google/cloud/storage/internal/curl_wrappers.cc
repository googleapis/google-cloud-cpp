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

#include "google/cloud/storage/internal/curl_wrappers.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

namespace {
class CurlInitializer {
 public:
  CurlInitializer() { curl_global_init(CURL_GLOBAL_ALL); }
  ~CurlInitializer() { curl_global_cleanup(); }
};

CurlInitializer const CURL_INITIALIZER;

extern "C" std::size_t WriteCallback(void* contents, std::size_t size,
                                     std::size_t nmemb, void* dest) {
  auto* buffer =
      reinterpret_cast<google::cloud::storage::internal::CurlBuffer*>(dest);
  buffer->Append(static_cast<char*>(contents), size * nmemb);
  return size * nmemb;
}

extern "C" std::size_t HeaderCallback(char* contents, std::size_t size,
                                      std::size_t nmemb, void* dest) {
  auto* headers =
      reinterpret_cast<google::cloud::storage::internal::CurlHeaders*>(dest);
  headers->Append(contents, size * nmemb);
  return size * nmemb;
}

}  // anonymous namespace

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
void CurlBuffer::Attach(CURL* curl) {
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
}

void CurlBuffer::Append(char* data, std::size_t size) {
  buffer_.append(data, size);
}

void CurlHeaders::Attach(CURL* curl) {
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
}

void CurlHeaders::Append(char* data, std::size_t size) {
  if (size <= 2) {
    // Empty header (including the \r\n), ignore.
    return;
  }
  if ('\r' != data[size - 2] or '\n' != data[size - 1]) {
    // Invalid header (should end in \r\n), ignore.
    return;
  }
  auto separator = std::find(data, data + size, ':');
  std::string header_name = std::string(data, separator);
  std::string header_value;
  // If there is a value, capture it, but ignore the final \r\n.
  if (static_cast<std::size_t>(separator - data) < size - 2) {
    header_value = std::string(separator + 2, data + size - 2);
  }
  std::transform(header_name.begin(), header_name.end(), header_name.begin(),
                 [](char x) { return std::tolower(x); });
  contents_.emplace(std::move(header_name), std::move(header_value));
}
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
