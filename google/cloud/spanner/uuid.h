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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_UUID_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_UUID_H

#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include "absl/numeric/int128.h"
#include "absl/strings/string_view.h"
#include <cstdint>
#include <iosfwd>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A representation of the Spanner UUID type: A fixed size 16 byte value
 * that can be represented as a 32-digit hexadecimal string.
 *
 * @see https://cloud.google.com/spanner/docs/data-types#uuid_type
 */
class Uuid {
 public:
  /// Default construction yields a zero value UUID.
  Uuid() = default;

  /// Construct a UUID from one unsigned 128-bit integer.
  explicit Uuid(absl::uint128 value);

  /// Construct a UUID from two unsigned 64-bit pieces.
  Uuid(std::uint64_t high_bits, std::uint64_t low_bits);

  /// @name Regular value type, supporting copy, assign, move.
  ///@{
  Uuid(Uuid&&) = default;
  Uuid& operator=(Uuid&&) = default;
  Uuid(Uuid const&) = default;
  Uuid& operator=(Uuid const&) = default;
  ///@}

  /// @name Relational operators
  ///
  ///@{
  friend bool operator==(Uuid const& lhs, Uuid const& rhs) {
    return lhs.uuid_ == rhs.uuid_;
  }
  friend bool operator!=(Uuid const& lhs, Uuid const& rhs) {
    return !(lhs == rhs);
  }
  friend bool operator<(Uuid const& lhs, Uuid const& rhs) {
    return lhs.uuid_ < rhs.uuid_;
  }
  friend bool operator<=(Uuid const& lhs, Uuid const& rhs) {
    return !(rhs < lhs);
  }
  friend bool operator>=(Uuid const& lhs, Uuid const& rhs) {
    return !(lhs < rhs);
  }
  friend bool operator>(Uuid const& lhs, Uuid const& rhs) { return rhs < lhs; }
  ///@}

  /// @name Returns a pair of unsigned 64-bit integers representing the UUID.
  std::pair<std::uint64_t, std::uint64_t> As64BitPair() const;

  /// @name Conversion to unsigned 128-bit integer representation.
  explicit operator absl::uint128() const { return uuid_; }

  /// @name Conversion to a lower case string formatted as:
  /// [8 hex-digits]-[4 hex-digits]-[4 hex-digits]-[4 hex-digits]-[12
  /// hex-digits]
  /// Example: 0b6ed04c-a16d-fc46-5281-7f9978c13738
  explicit operator std::string() const;

  /// @name Output streaming
  friend std::ostream& operator<<(std::ostream& os, Uuid uuid) {
    return os << std::string(uuid);
  }

 private:
  absl::uint128 uuid_ = 0;
};

/**
 * Parses a textual representation a `Uuid` from a string of hexadecimal digits.
 * Returns an error if unable to parse the given input.
 *
 * Acceptable input strings must consist of 32 hexadecimal digits: [0-9a-fA-F].
 * Optional curly braces are allowed around the entire sequence of digits as are
 * hyphens between any pair of hexadecimal digits.
 *
 * Example acceptable inputs:
 *  - {0b6ed04c-a16d-fc46-5281-7f9978c13738}
 *  - 0b6ed04ca16dfc4652817f9978c13738
 *  - 7Bf8-A7b8-1917-1919-2625-F208-c582-4254
 *  - {DECAFBAD-DEAD-FADE-CAFE-FEEDFACEBEEF}
 */
StatusOr<Uuid> MakeUuid(absl::string_view s);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_UUID_H
