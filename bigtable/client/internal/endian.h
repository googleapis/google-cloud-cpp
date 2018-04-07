// Copyright 2018 Google LLC.
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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_ENDIAN_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_ENDIAN_H_

#include <bitset>
#include <limits>
#include <vector>

#include "bigtable/client/internal/strong_type.h"
#include "bigtable/client/internal/throw_delegate.h"

#ifdef _MSC_VER
#include <stdlib.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <byteswap.h>
#endif

#include <string>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
using bigendian64_t = internal::StrongType<std::int64_t, struct BigEndianType>;

namespace internal {

#ifndef _MSC_VER
#ifndef __GNUC__
#ifndef __clang__
static std::int64_t explicit_swap64(std::int64_t value) {
  std::int64_t return_value = ((((value)&0xff00000000000000ull) >> 56) |
                               (((value)&0x00ff000000000000ull) >> 40) |
                               (((value)&0x0000ff0000000000ull) >> 24) |
                               (((value)&0x000000ff00000000ull) >> 8) |
                               (((value)&0x00000000ff000000ull) << 8) |
                               (((value)&0x0000000000ff0000ull) << 24) |
                               (((value)&0x000000000000ff00ull) << 40) |
                               (((value)&0x00000000000000ffull) << 56));

  return return_value;
}
#endif
#endif
#endif

inline bigtable::bigendian64_t byteswap64(bigtable::bigendian64_t value) {
#ifdef _MSC_VER
  return bigtable::bigendian64_t(_byteswap_uint64(value.get()));
#elif defined(__GNUC__) || defined(__clang__)
  return bigtable::bigendian64_t(__builtin_bswap64(value.get()));
#else
  return bigtable::bigendian64_t(explicit_swap64(value.get()));
#endif
}

constexpr char c[sizeof(int)] = {1};
constexpr bool IsBigEndian() {
  return c[0] != 1;  // ignore different type comparison
}

template <typename T>
struct encoder {
  static std::string encode(T const& value);
  static T decode(std::string const& value);
};

template <>
struct encoder<bigtable::bigendian64_t> {
  /**
   * Convert BigEndian numeric value into a string of bytes and return it.
   *
   * Google Cloud Bigtable stores arbitrary blobs in each cell. These
   * blobs are stored in a cell as a string of bytes. Values need to be
   * converted to/from these cell blobs bytes to be used in application.
   * This function is used to convert BigEndian 64 bit numeric value into
   * string of bytes, so that it could be stored as a cell blob.
   * For this conversion we consider the char size is 8 bits.
   * So If machine architecture does not define char as 8 bit then we
   * raise the assert.
   *
   * @param value reference to a number need to be converted into
   *        string of bytes.
   */
  static std::string encode(bigtable::bigendian64_t const& value) {
    static_assert(std::numeric_limits<unsigned char>::digits == 8,
                  "This code assumes char is an 8-bit number");

    std::string big_endian_string("", 8);
    if (IsBigEndian()) {
      std::memcpy((void*)big_endian_string.data(), &value, sizeof(value));
    } else {
      bigtable::bigendian64_t swapped_value = byteswap64(value);
      std::memcpy((void*)big_endian_string.data(), &swapped_value,
                  sizeof(swapped_value));
    }
    return big_endian_string;
  }

  /**
   * Convert BigEndian string of bytes into BigEndian numeric value and
   * return it.
   *
   * Google Cloud Bigtable stores arbitrary blobs in each cell. These
   * blobs are stored in a cell as a string of bytes. Values need to be
   * converted to/from these cell blobs bytes to be used in application.
   * This function is used to convert string of bytes into BigEndian 64
   * bit numeric value. so that it could be used in application.
   * For this conversion we consider the char size is 8 bits.
   * So If machine architecture does not define char as 8 bit then we
   * raise the std::range_error.
   *
   * @param value reference to a string of bytes need to be converted into
   *        BigEndian numeric value.
   */
  static bigtable::bigendian64_t decode(std::string const& value) {
    // Check if value is BigEndian 64-bit integer
    if (value.size() != 8) {
      internal::RaiseRangeError("Value is not convertible to uint64");
    }
    bigtable::bigendian64_t big_endian_value(0);
    std::memcpy(&big_endian_value, value.c_str(), 8);
    if (!IsBigEndian()) {
      big_endian_value = byteswap64(big_endian_value);
    }
    return big_endian_value;
  }
};

}  // namespace internal

inline std::string as_bigendian64(bigtable::bigendian64_t value) {
  return internal::encoder<bigtable::bigendian64_t>::encode(value);
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_TABLE_H_
