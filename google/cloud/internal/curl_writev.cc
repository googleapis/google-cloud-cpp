// Copyright 2023 Google LLC
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

#include "google/cloud/internal/curl_writev.h"
#include <cstdio>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

bool WriteVector::Seek(std::size_t offset, int origin) {
  // libcurl claims to only req
  if (origin != SEEK_SET) return false;
  writev_.assign(original_.begin(), original_.end());
  while (!writev_.empty()) {
    auto& src = writev_.front();
    if (src.size() >= offset) {
      src.remove_prefix(offset);
      offset = 0;
      break;
    }
    offset -= src.size();
    writev_.pop_front();
  }
  return offset == 0;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
