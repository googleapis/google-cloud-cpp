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

#include "storage/client/internal/curl_wrappers.h"
#include <algorithm>
#include <iostream>

namespace {
class CurlInitializer {
 public:
  CurlInitializer() { curl_global_init(CURL_GLOBAL_ALL); }
  ~CurlInitializer() { curl_global_cleanup(); }
};

CurlInitializer CURL_INITIALIZER;

extern "C" std::size_t WriteCallback(void* contents, std::size_t size,
                                     std::size_t nmemb, void* dest) {
  auto* buffer = reinterpret_cast<storage::internal::CurlBuffer*>(dest);
  buffer->Append(static_cast<char*>(contents), size * nmemb);
  return size * nmemb;
}

extern "C" std::size_t HeaderCallback(char* contents, std::size_t size,
                                      std::size_t nmemb, void* dest) {
  auto* headers = reinterpret_cast<storage::internal::CurlHeaders*>(dest);
  headers->Append(contents, size * nmemb);
  return size * nmemb;
}

}  // anonymous namespace

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
  auto separator = std::find(data, data + size, ':');
  std::string header_name = std::string(data, separator);
  std::string header_value{};
  if (separator - data < size) {
    header_value = std::string(separator + 1, data + size);
  }
  contents_.emplace(std::move(header_name), std::move(header_value));
}
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // storage
