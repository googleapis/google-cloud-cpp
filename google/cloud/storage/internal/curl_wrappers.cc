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
#include "google/cloud/log.h"
#include "google/cloud/storage/internal/binary_data_as_debug_string.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
/// Automatically initialize (and cleanup) the libcurl library.
class CurlInitializer {
 public:
  CurlInitializer() { curl_global_init(CURL_GLOBAL_ALL); }
  ~CurlInitializer() { curl_global_cleanup(); }
};

CurlInitializer const CURL_INITIALIZER;

}  // namespace

std::size_t CurlAppendHeaderData(CurlReceivedHeaders& received_headers,
                                 char const* data, std::size_t size) {
  if (size <= 2) {
    // Empty header (including the \r\n), ignore.
    return size;
  }
  if ('\r' != data[size - 2] or '\n' != data[size - 1]) {
    // Invalid header (should end in \r\n), ignore.
    return size;
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
  received_headers.emplace(std::move(header_name), std::move(header_value));
  return size;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
