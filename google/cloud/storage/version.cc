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

#include "google/cloud/storage/version.h"
#include "google/cloud/internal/api_client_header.h"
#include <limits>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
std::string version_string() { return ::google::cloud::version_string(); }

std::string x_goog_api_client() {
  return google::cloud::internal::ApiClientHeader();
}

// These were sprinkled through the code, consolidated here because I could
// not find a better place.
auto constexpr kExpectedCharDigits = 8;
static_assert(
    std::numeric_limits<unsigned char>::digits == kExpectedCharDigits,
    "The Google Cloud Storage C++ library is only supported on platforms\n"
    "with 8-bit chars.  Please file a bug on\n"
    "    https://github.com/googleapis/google-cloud-cpp/issues\n"
    "describing your platform details to request support for it.");

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
