// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_BIG_ENDIAN_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_BIG_ENDIAN_H_

#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

// Encodes an 8-bit, 16-bit, 32-bit, or 64-bit signed or unsigned value as a
// big-endian sequence of bytes. The returned string has a size matching
// `sizeof(T)`. Example:
//
//   std::string s = EncodeBigEndian(std::int32_t{255});
//   assert(s == std::string("\0\0\0\xFF", 4));
//
template <typename T,
          typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
inline std::string EncodeBigEndian(T value) {
  static_assert(std::numeric_limits<unsigned char>::digits == 8,
                "This code assumes an 8-bit char");
  using unsigned_type = typename std::make_unsigned<T>::type;
  unsigned_type const n = value;
  unsigned_type shift = sizeof(n) * 8;
  std::string s(sizeof(n), '\0');
  for (auto& c : s) {
    shift -= 8;
    c = (n >> shift) & 0xFF;
  }
  return s;
}

// Decodes the given string as a big-endian sequence of bytes representing an
// integer of the specified type. The allowed types are std::int8_t through
// std::int64_t, both signed and unsigned. Returns an error status if the given
// string is the wrong size for the specified type. Example:
//
//   std::string s("\0\0\0\xFF", 4);
//   StatusOr<std::int32_t> decoded = DecodeBigEndian(s);
//   if (decoded) assert(*decoded == 255);
//
template <typename T,
          typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
inline StatusOr<T> DecodeBigEndian(std::string const& value) {
  static_assert(std::numeric_limits<unsigned char>::digits == 8,
                "This code assumes an 8-bit char");
  if (value.size() != sizeof(T)) {
    return Status(StatusCode::kInvalidArgument,
                  "Value must be sizeof(T) bytes long");
  }
  using unsigned_type = typename std::make_unsigned<T>::type;
  unsigned_type shift = sizeof(T) * 8;
  T result = 0;
  for (auto const& c : value) {
    shift -= 8;
    result |= (c & unsigned_type{0xFF}) << shift;
  }
  return result;
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_BIG_ENDIAN_H_
