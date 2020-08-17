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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_BIG_ENDIAN_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_BIG_ENDIAN_H

#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

// Encodes signed or unsigned integers as a big-endian sequence of bytes. The
// returned string has a size matching `sizeof(T)`. Example:
//
//   std::string s = EncodeBigEndian(std::int32_t{255});
//   assert(s == std::string("\0\0\0\xFF", 4));
//
template <typename T,
          typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
std::string EncodeBigEndian(T value) {
  static_assert(std::numeric_limits<unsigned char>::digits == 8,
                "This code assumes an 8-bit char");
  using unsigned_type = typename std::make_unsigned<T>::type;
  unsigned_type const n = *reinterpret_cast<unsigned_type*>(&value);
  auto shift = sizeof(n) * 8;
  std::array<std::uint8_t, sizeof(n)> a;
  for (auto& c : a) {
    shift -= 8;
    c = static_cast<std::uint8_t>(n >> shift);
  }
  return {reinterpret_cast<char const*>(a.data()), a.size()};
}

// Decodes the given string as a big-endian sequence of bytes representing an
// integer of the specified type. Returns an error status if the given string
// is the wrong size for the specified type. Example:
//
//   std::string s("\0\0\0\xFF", 4);
//   StatusOr<std::int32_t> decoded = DecodeBigEndian(s);
//   if (decoded) assert(*decoded == 255);
//
template <typename T,
          typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
StatusOr<T> DecodeBigEndian(std::string const& value) {
  static_assert(std::numeric_limits<unsigned char>::digits == 8,
                "This code assumes an 8-bit char");
  if (value.size() != sizeof(T)) {
    auto const msg = "Given value with " + std::to_string(value.size()) +
                     " bytes; expected " + std::to_string(sizeof(T));
    return Status(StatusCode::kInvalidArgument, msg);
  }
  using unsigned_type = typename std::make_unsigned<T>::type;
  auto shift = sizeof(T) * 8;
  unsigned_type result = 0;
  for (auto const& c : value) {
    auto const n = *reinterpret_cast<std::uint8_t const*>(&c);
    shift -= 8;
    result = static_cast<unsigned_type>(result | (unsigned_type{n} << shift));
  }
  return *reinterpret_cast<T*>(&result);
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_BIG_ENDIAN_H
