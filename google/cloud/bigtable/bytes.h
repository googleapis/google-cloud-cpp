// Copyright 2025 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BYTES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BYTES_H

#include "google/cloud/bigtable/version.h"
#include "google/cloud/status_or.h"
#include <array>
#include <cstddef>
#include <ostream>
#include <string>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
struct BytesInternals;
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal

namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A representation of the Bigtable BYTES type: variable-length binary data.
 *
 * A `Bytes` value can be constructed from, and converted to any sequence of
 * octets. `Bytes` values can be compared for equality.
 */
class Bytes {
 public:
  /// An empty sequence.
  Bytes() = default;

  /// @name Construction from a sequence of octets.
  ///@{
  template <typename InputIt>
  Bytes(InputIt first, InputIt last) {
    bytes_ = std::string(first, last);
  }
  template <typename Container>
  explicit Bytes(Container const& c) : Bytes(std::begin(c), std::end(c)) {}
  ///@}

  /// Conversion to a sequence of octets.  The `Container` must support
  /// construction from a range specified as a pair of input iterators.
  template <typename Container>
  Container get() const {
    return bytes_;
  }

  /// @name Relational operators
  ///@{
  friend bool operator==(Bytes const& a, Bytes const& b) {
    return a.bytes_ == b.bytes_;
  }
  friend bool operator!=(Bytes const& a, Bytes const& b) { return !(a == b); }
  friend bool operator<(Bytes const& a, Bytes const& b) {
    return a.bytes_ < b.bytes_;
  }
  friend bool operator>=(Bytes const& a, Bytes const& b) { return !(a < b); }
  friend bool operator>(Bytes const& a, Bytes const& b) { return !(a <= b); }
  friend bool operator<=(Bytes const& a, Bytes const& b) { return !(b < a); }
  ///@}

  /**
   * Outputs string representation of the Bytes to the provided stream.
   *
   * @warning This is intended for debugging and human consumption only, not
   *     machine consumption, as the output format may change without notice.
   */
  friend std::ostream& operator<<(std::ostream& os, Bytes const& bytes);

 private:
  friend struct bigtable_internal::BytesInternals;

  std::string bytes_;  // raw bytes
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable

}  // namespace cloud
}  // namespace google

template <>
struct std::hash<google::cloud::bigtable::Bytes> {
  std::size_t operator()(
      google::cloud::bigtable::Bytes const& b) const noexcept {
    return std::hash<std::string>()(b.get<std::string>());
  }
};

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BYTES_H
