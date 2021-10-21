// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CONST_BUFFER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CONST_BUFFER_H

#include "google/cloud/storage/version.h"
#include "absl/types/span.h"
#include <numeric>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

/// Represent a memory range. Use to upload with low copying
using ConstBuffer = absl::Span<char const>;

/// Represent a sequence of memory ranges. Use to upload with low copying.
using ConstBufferSequence = std::vector<ConstBuffer>;

/// The total number of bytes in the buffer sequence.
inline std::size_t TotalBytes(ConstBufferSequence const& s) {
  return std::accumulate(
      s.begin(), s.end(), std::size_t{0},
      [](std::size_t a, ConstBuffer const& b) { return a + b.size(); });
}

/// Remove @p count bytes at the start of @p s
void PopFrontBytes(ConstBufferSequence& s, std::size_t count);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CONST_BUFFER_H
