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

#include "bigtable/client/internal/endian.h"

#ifdef _MSC_VER
#include <stdlib.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <byteswap.h>
#endif

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

constexpr char c[sizeof(int)] = {1};
constexpr bool IsBigEndian() {
  return c[0] != 1;  // ignore different type comparison
}

/**
 * Convert BigEndian numeric value into a string of bytes and return it.
 */
template <>
std::string Encoder<bigtable::bigendian64_t>::Encode(
    bigtable::bigendian64_t const& value) {
  static_assert(std::numeric_limits<unsigned char>::digits ==
                    sizeof(bigtable::bigendian64_t),
                "This code assumes char is an 8-bit number");

  std::string big_endian_string("", sizeof(bigtable::bigendian64_t));
  if (IsBigEndian()) {
    std::memcpy(static_cast<void*>((void*)big_endian_string.data()), &value,
                sizeof(value));
  } else {
    bigtable::bigendian64_t swapped_value = byteswap64(value);
    std::memcpy(static_cast<void*>((void*)big_endian_string.data()),
                &swapped_value, sizeof(swapped_value));
  }
  return big_endian_string;
}

/**
 * Convert BigEndian string of bytes into BigEndian numeric value and return it.
 */
template <>
bigtable::bigendian64_t Encoder<bigtable::bigendian64_t>::Decode(
    std::string const& value) {
  // Check if value is BigEndian 64-bit integer
  if (value.size() != sizeof(bigtable::bigendian64_t)) {
    internal::RaiseRangeError("Value is not convertible to uint64");
  }
  bigtable::bigendian64_t big_endian_value(0);
  std::memcpy(&big_endian_value, value.c_str(),
              sizeof(bigtable::bigendian64_t));
  if (!IsBigEndian()) {
    big_endian_value = byteswap64(big_endian_value);
  }
  return big_endian_value;
}

inline bigtable::bigendian64_t byteswap64(bigtable::bigendian64_t value) {
#ifdef _MSC_VER
  return bigtable::bigendian64_t(_byteswap_uint64(value.get()));
#elif defined(__GNUC__) || defined(__clang__)
  return bigtable::bigendian64_t(__builtin_bswap64(value.get()));
#else
  std::int64_t num_value = value.get();

  std::int64_t return_value = ((((num_value)&0xff00000000000000ull) >> 56) |
                               (((num_value)&0x00ff000000000000ull) >> 40) |
                               (((num_value)&0x0000ff0000000000ull) >> 24) |
                               (((num_value)&0x000000ff00000000ull) >> 8) |
                               (((num_value)&0x00000000ff000000ull) << 8) |
                               (((num_value)&0x0000000000ff0000ull) << 24) |
                               (((num_value)&0x000000000000ff00ull) << 40) |
                               (((num_value)&0x00000000000000ffull) << 56));

  return bigtable::bigendian64_t(return_value);
#endif
}

std::string as_bigendian64(bigtable::bigendian64_t value) {
  return bigtable::internal::Encoder<bigtable::bigendian64_t>::Encode(value);
}

}  // namespace internal

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
