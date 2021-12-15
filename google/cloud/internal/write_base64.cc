// Copyright 2020 Google LLC
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

#include "google/cloud/internal/write_base64.h"
#include "google/cloud/internal/openssl_util.h"
#include <fstream>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

void WriteBase64AsBinary(std::string const& filename, char const* data) {
  std::ofstream os(filename, std::ios::binary);
  os.exceptions(std::ios::badbit);
  auto bytes = internal::Base64Decode(data).value();
  for (unsigned char c : bytes) {
    os << c;
  }
  os.close();
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
