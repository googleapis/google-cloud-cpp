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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_WRITEV_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_WRITEV_H

#include "google/cloud/version.h"
#include "absl/types/span.h"
#include <deque>
#include <vector>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Vector of data chunks to satisfy requests from libcurl.
class WriteVector {
 public:
  WriteVector() = default;
  explicit WriteVector(std::vector<absl::Span<char const>> v)
      : original_(std::move(v)), writev_(original_.begin(), original_.end()) {}

  /// Returns the number of bytes available in the write vector.
  std::size_t size() const;

  /// Copies as much data as possible from the internal vector to @p dst.
  ///
  /// @return the number of bytes copied.
  std::size_t MoveTo(absl::Span<char> dst);

  /// Implements a CURLOPT_SEEKFUNCTION callback.
  ///
  /// @see https://curl.se/libcurl/c/CURLOPT_SEEKFUNCTION.html
  /// @returns true if the seek operation was successful.
  bool Seek(std::size_t offset, int origin);

 private:
  std::vector<absl::Span<char const>> original_;
  std::deque<absl::Span<char const>> writev_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_WRITEV_H
