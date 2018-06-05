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

#include "google/cloud/bigtable/internal/endian.h"
#include "google/cloud/internal/throw_delegate.h"
#include <cstring>
#include <limits>

#ifdef _MSC_VER
#include <stdlib.h>
#elif defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <byteswap.h>
#endif

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

constexpr char ENDIAN_DETECTOR[sizeof(int)] = {1};
constexpr bool IsBigEndian() {
  return ENDIAN_DETECTOR[0] != 1;  // ignore different type comparison
}

/**
 * Convert BigEndian numeric value into a string of bytes and return it.
 */
std::string Encoder<bigtable::bigendian64_t>::Encode(
    bigtable::bigendian64_t const& value) {
  static_assert(std::numeric_limits<unsigned char>::digits ==
                    sizeof(bigtable::bigendian64_t),
                "This code assumes char is an 8-bit number");

  char big_endian_buffer[sizeof(bigtable::bigendian64_t) + 1] = "";
  if (IsBigEndian()) {
    std::memcpy(&big_endian_buffer, &value, sizeof(value));
  } else {
    bigtable::bigendian64_t swapped_value = ByteSwap64(value);
    std::memcpy(&big_endian_buffer, &swapped_value, sizeof(swapped_value));
  }

  return std::string(big_endian_buffer, sizeof(bigtable::bigendian64_t));
}

/**
 * Convert BigEndian string of bytes into BigEndian numeric value and return it.
 */
bigtable::bigendian64_t Encoder<bigtable::bigendian64_t>::Decode(
    std::string const& value) {
  // Check if value is BigEndian 64-bit integer
  if (value.size() != sizeof(bigtable::bigendian64_t)) {
    google::cloud::internal::RaiseRangeError(
        "Value is not convertible to uint64");
  }
  bigtable::bigendian64_t big_endian_value(0);
  std::memcpy(&big_endian_value, value.c_str(),
              sizeof(bigtable::bigendian64_t));
  if (!IsBigEndian()) {
    big_endian_value = ByteSwap64(big_endian_value);
  }
  return big_endian_value;
}

inline bigtable::bigendian64_t ByteSwap64(bigtable::bigendian64_t value) {
#ifdef _MSC_VER
  return bigtable::bigendian64_t(_byteswap_uint64(value.get()));
#elif defined(__APPLE__)
  return bigtable::bigendian64_t(OSSwapInt64(value.get()));
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

std::string AsBigEndian64(bigtable::bigendian64_t value) {
  return bigtable::internal::Encoder<bigtable::bigendian64_t>::Encode(value);
}

}  // namespace internal

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
