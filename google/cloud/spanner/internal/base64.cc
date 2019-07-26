// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/internal/base64.h"
#include <array>
#include <climits>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

constexpr char kPadding = '=';

constexpr std::array<char, 64> kIndexToChar = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/',
};

constexpr std::array<unsigned char, UCHAR_MAX + 1> kCharToIndexExcessOne = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  63, 0,  0,  0,  64, 53, 54, 55, 56, 57, 58,
    59, 60, 61, 62, 0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,  7,
    8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
    26, 0,  0,  0,  0,  0,  0,  27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37,
    38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
};

// UCHAR_MAX is required to be at least 255, meaning std::string::value_type
// can always hold an octet. If UCHAR_MAX > 255, however, we have no way to
// base64 encode large values. So, we demand exactly 255.
static_assert(UCHAR_MAX == 255, "required by Base64Encode()");

}  // namespace

std::string Base64Encode(std::string const& bytes) {
  std::string encoded;
  auto* p = reinterpret_cast<unsigned char const*>(bytes.data());
  auto* const ep = p + bytes.size();
  encoded.reserve((ep - p + 2) / 3 * 4);  // 3 octets to 4 sextets
  while (ep - p >= 3) {
    unsigned int const v = p[0] << 16 | p[1] << 8 | p[2];
    encoded.push_back(kIndexToChar[v >> 18]);
    encoded.push_back(kIndexToChar[v >> 12 & 0x3f]);
    encoded.push_back(kIndexToChar[v >> 6 & 0x3f]);
    encoded.push_back(kIndexToChar[v & 0x3f]);
    p += 3;
  }
  switch (ep - p) {
    case 2: {
      unsigned int const v = p[0] << 16 | p[1] << 8;
      encoded.push_back(kIndexToChar[v >> 18]);
      encoded.push_back(kIndexToChar[v >> 12 & 0x3f]);
      encoded.push_back(kIndexToChar[v >> 6 & 0x3f]);
      encoded.push_back(kPadding);
      break;
    }
    case 1: {
      unsigned int const v = p[0] << 16;
      encoded.push_back(kIndexToChar[v >> 18]);
      encoded.push_back(kIndexToChar[v >> 12 & 0x3f]);
      encoded.append(2, kPadding);
      break;
    }
  }
  return encoded;
}

StatusOr<std::string> Base64Decode(std::string const& base64) {
  std::string decoded;
  auto* p = reinterpret_cast<unsigned char const*>(base64.data());
  auto* ep = p + base64.size();
  decoded.reserve((ep - p + 3) / 4 * 3);  // 4 sextets to 3 octets
  while (ep - p >= 4) {
    auto i0 = kCharToIndexExcessOne[p[0]];
    auto i1 = kCharToIndexExcessOne[p[1]];
    if (--i0 >= 64 || --i1 >= 64) break;
    if (p[3] == kPadding) {
      if (p[2] == kPadding) {
        if ((i1 & 0xf) != 0) break;
        decoded.push_back(i0 << 2 | i1 >> 4);
      } else {
        auto i2 = kCharToIndexExcessOne[p[2]];
        if (--i2 >= 64 || (i2 & 0x3) != 0) break;
        decoded.push_back(i0 << 2 | i1 >> 4);
        decoded.push_back(i1 << 4 | i2 >> 2);
      }
      p += 4;
      break;
    }
    auto i2 = kCharToIndexExcessOne[p[2]];
    auto i3 = kCharToIndexExcessOne[p[3]];
    if (--i2 >= 64 || --i3 >= 64) break;
    decoded.push_back(i0 << 2 | i1 >> 4);
    decoded.push_back(i1 << 4 | i2 >> 2);
    decoded.push_back(i2 << 6 | i3);
    p += 4;
  }
  if (p != ep) {
    auto const offset = reinterpret_cast<char const*>(p) - base64.data();
    auto const bad_chunk = base64.substr(offset, 4);
    auto message = "Invalid base64 chunk \"" + bad_chunk + "\"" +
                   " at offset " + std::to_string(offset);
    return Status(StatusCode::kInvalidArgument, std::move(message));
  }
  return decoded;
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
