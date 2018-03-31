// Copyright 2018 Google Inc.
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
#include <assert.h>
#include <vector>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
std::string NumericToBigEndian(std::uint64_t value) {
  // Store big endian representation in a vector:
  std::vector<unsigned char> bigEndian;
  for (int index = 7; index >= 0; index--) {
    bigEndian.push_back((value >> (8 * index)) & 0xff);
  }

  // Convert vector of chars to string:
  std::string bigEndianString(bigEndian.begin(), bigEndian.end());
  return bigEndianString;
}

std::uint64_t BigEndianToNumeric(std::string value) {
  // Check if value_ is BigEndian 64-bit integer
  assert(value.size() == 8 && "Value is not convertible to uint64");

  auto bigendian64 = [](char const* buf) -> std::uint64_t {
    auto bigendian32 = [](char const* buf) -> std::uint32_t {
      auto bigendian16 = [](char const* buf) -> std::uint16_t {
        return (static_cast<std::uint16_t>(buf[0]) << 8) + std::uint8_t(buf[1]);
      };
      return (static_cast<std::uint32_t>(bigendian16(buf)) << 16) +
             bigendian16(buf + 2);
    };
    return (static_cast<std::uint64_t>(bigendian32(buf)) << 32) +
           bigendian32(buf + 4);
  };

  return bigendian64(value.c_str());
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
