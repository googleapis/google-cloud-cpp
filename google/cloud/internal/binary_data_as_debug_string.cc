// Copyright 2018 Google LLC
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

#include "google/cloud/internal/binary_data_as_debug_string.h"
#include <algorithm>
#include <cctype>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string BinaryDataAsDebugString(char const* data, std::size_t size,
                                    std::size_t max_output_bytes) {
  auto dump = std::string{
      data, max_output_bytes == 0 ? size : std::min(size, max_output_bytes)};
  std::transform(dump.begin(), dump.end(), dump.begin(),
                 [](auto c) { return std::isprint(c) ? c : '.'; });
  if (max_output_bytes == 0 || size <= max_output_bytes) return dump;
  return dump + "...<truncated>...";
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
