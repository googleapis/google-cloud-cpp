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

#include "google/cloud/version.h"
#ifdef _MSC_VER
#include <stdlib.h>
#elif defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <byteswap.h>
#endif
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
// integer of the specified type. The allowed types std::int8_t through
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
    return Status(StatusCode::kInvalidArgument, "Value must be 8 bytes long");
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

constexpr char ENDIAN_DETECTOR[sizeof(int)] = {1};
constexpr bool IsBigEndian() {
  return ENDIAN_DETECTOR[0] != 1;  // ignore different type comparison
}

template <bool is_native_bigendian>
struct EndianTransform {
  template <typename IntegralType>
  static constexpr IntegralType as_big_endian(IntegralType native) {
    return native;
  }

  template <typename IntegralType>
  static constexpr IntegralType as_native(IntegralType big_endian) {
    return big_endian;
  }
};

template <>
struct EndianTransform<false> {
  static inline std::int16_t byte_swap(std::int16_t value) {
#ifdef _MSC_VER
    return _byteswap_ushort(value);
#elif defined(__APPLE__)
    return OSSwapInt16(value);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16(value);
#else
    std::int16_t return_value =
        ((((num_value)&0xff00u) >> 8) | (((num_value)&0x00ffu) << 8));

    return return_value;
#endif
  }

  static inline std::int32_t byte_swap(std::int32_t value) {
#ifdef _MSC_VER
    return _byteswap_ulong(value);
#elif defined(__APPLE__)
    return OSSwapInt32(value);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(value);
#else
    std::int32_t return_value =
        ((((num_value)&0xff000000ul) >> 24) |
         (((num_value)&0x00ff0000ul) >> 8) | (((num_value)&0x0000ff00ul) << 8) |
         (((num_value)&0x000000fful) << 24));

    return return_value;
#endif
  }

  static inline std::int64_t byte_swap(std::int64_t value) {
#ifdef _MSC_VER
    return _byteswap_uint64(value);
#elif defined(__APPLE__)
    return OSSwapInt64(value);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(value);
#else
    std::int64_t return_value = ((((num_value)&0xff00000000000000ull) >> 56) |
                                 (((num_value)&0x00ff000000000000ull) >> 40) |
                                 (((num_value)&0x0000ff0000000000ull) >> 24) |
                                 (((num_value)&0x000000ff00000000ull) >> 8) |
                                 (((num_value)&0x00000000ff000000ull) << 8) |
                                 (((num_value)&0x0000000000ff0000ull) << 24) |
                                 (((num_value)&0x000000000000ff00ull) << 40) |
                                 (((num_value)&0x00000000000000ffull) << 56));

    return return_value;
#endif
  }

  static inline std::uint16_t byte_swap(std::uint16_t value) {
#ifdef _MSC_VER
    return _byteswap_ushort(value);
#elif defined(__APPLE__)
    return OSSwapInt16(value);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16(value);
#else
    std::uint16_t return_value =
        ((((num_value)&0xff00u) >> 8) | (((num_value)&0x00ffu) << 8));

    return return_value;
#endif
  }

  static inline std::uint32_t byte_swap(std::uint32_t value) {
#ifdef _MSC_VER
    return _byteswap_ulong(value);
#elif defined(__APPLE__)
    return OSSwapInt32(value);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(value);
#else
    std::uint32_t return_value =
        ((((num_value)&0xff000000ul) >> 24) |
         (((num_value)&0x00ff0000ul) >> 8) | (((num_value)&0x0000ff00ul) << 8) |
         (((num_value)&0x000000fful) << 24));

    return return_value;
#endif
  }

  static inline std::uint64_t byte_swap(std::uint64_t value) {
#ifdef _MSC_VER
    return _byteswap_uint64(value);
#elif defined(__APPLE__)
    return OSSwapInt64(value);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(value);
#else
    std::uint64_t return_value = ((((num_value)&0xff00000000000000ull) >> 56) |
                                  (((num_value)&0x00ff000000000000ull) >> 40) |
                                  (((num_value)&0x0000ff0000000000ull) >> 24) |
                                  (((num_value)&0x000000ff00000000ull) >> 8) |
                                  (((num_value)&0x00000000ff000000ull) << 8) |
                                  (((num_value)&0x0000000000ff0000ull) << 24) |
                                  (((num_value)&0x000000000000ff00ull) << 40) |
                                  (((num_value)&0x00000000000000ffull) << 56));

    return return_value;
#endif
  }

  template <typename IntegralType>
  static inline IntegralType as_big_endian(IntegralType native) {
    return byte_swap(native);
  }

  template <typename IntegralType>
  static inline IntegralType as_native(IntegralType big_endian) {
    return byte_swap(big_endian);
  }
};

template <typename IntegralType>
constexpr IntegralType ToBigEndian(IntegralType native) {
  return EndianTransform<IsBigEndian()>::as_big_endian(native);
}

template <typename IntegralType>
constexpr IntegralType FromBigEndian(IntegralType big_endian) {
  return EndianTransform<IsBigEndian()>::as_native(big_endian);
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_BIG_ENDIAN_H_
