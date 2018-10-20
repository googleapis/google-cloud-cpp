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
#include "google/cloud/internal/big_endian.h"
#include "google/cloud/internal/throw_delegate.h"
#include <cstring>
#include <limits>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * Convert BigEndian numeric value into a string of bytes and return it.
 */
std::string Encoder<bigtable::bigendian64_t>::Encode(
    bigtable::bigendian64_t const& value) {
  static_assert(std::numeric_limits<unsigned char>::digits ==
                    sizeof(bigtable::bigendian64_t),
                "This code assumes char is an 8-bit number");

  char big_endian_buffer[sizeof(bigtable::bigendian64_t) + 1] = "";
  if (google::cloud::internal::IsBigEndian()) {
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
  if (!google::cloud::internal::IsBigEndian()) {
    big_endian_value = ByteSwap64(big_endian_value);
  }
  return big_endian_value;
}

inline bigtable::bigendian64_t ByteSwap64(bigtable::bigendian64_t value) {
  return bigtable::bigendian64_t(
      google::cloud::internal::ToBigEndian(value.get()));
}

std::string AsBigEndian64(bigtable::bigendian64_t value) {
  return bigtable::internal::Encoder<bigtable::bigendian64_t>::Encode(value);
}

}  // namespace internal

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
