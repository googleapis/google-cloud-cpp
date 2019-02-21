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
#include <limits>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * Convert a numeric value into a string of bytes and return it.
 */
std::string Encoder<std::int64_t>::Encode(std::int64_t const& value) {
  static_assert(sizeof(std::int64_t) == 8,
                "This code assumes an 8-byte integer");
  static_assert(std::numeric_limits<unsigned char>::digits == 8,
                "This code assumes an 8-bit char");
  uint64_t const n = value;
  std::string s(8, '0');
  s[0] = (n >> 56) & 0xFF;
  s[1] = (n >> 48) & 0xFF;
  s[2] = (n >> 40) & 0xFF;
  s[3] = (n >> 32) & 0xFF;
  s[4] = (n >> 24) & 0xFF;
  s[5] = (n >> 16) & 0xFF;
  s[6] = (n >> 8) & 0xFF;
  s[7] = (n >> 0) & 0xFF;
  return s;
}

/**
 * Convert BigEndian string of bytes into BigEndian numeric value and return it.
 */
std::int64_t Encoder<std::int64_t>::Decode(std::string const& value) {
  static_assert(sizeof(std::int64_t) == 8,
                "This code assumes an 8-byte integer");
  static_assert(std::numeric_limits<unsigned char>::digits == 8,
                "This code assumes an 8-bit char");
  if (value.size() != 8) {
    google::cloud::internal::ThrowRangeError("Value must be 8 bytes");
  }
  return ((value[7] & uint64_t{0xFF}) << 0) |
         ((value[6] & uint64_t{0xFF}) << 8) |
         ((value[5] & uint64_t{0xFF}) << 16) |
         ((value[4] & uint64_t{0xFF}) << 24) |
         ((value[3] & uint64_t{0xFF}) << 32) |
         ((value[2] & uint64_t{0xFF}) << 40) |
         ((value[1] & uint64_t{0xFF}) << 48) |
         ((value[0] & uint64_t{0xFF}) << 56);
}

std::string AsBigEndian64(std::int64_t value) {
  return bigtable::internal::Encoder<std::int64_t>::Encode(value);
}

}  // namespace internal

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
