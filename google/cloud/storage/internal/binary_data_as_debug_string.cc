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

#include "google/cloud/storage/internal/binary_data_as_debug_string.h"
#include <array>
#include <cctype>
#include <limits>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
std::string BinaryDataAsDebugString(char const* data, std::size_t size,
                                    std::size_t max_output_bytes) {
  // We want about 2/3 of a standard 80 column terminal to be used by the hex
  // representation and the other 1/3 (because it is half as wide) with the
  // text representation. Setting this value to 24 uses 72 columns: 48 for the
  // hex representation, 24 for text, and one space. We could use 25 or 26, we
  // chose (somewhat arbitrarily) 24 as it is 16 + 8 and thus more "round" in
  // base 2.
  auto constexpr kTextWidth = 24;
  std::string result;
  std::string text_column(kTextWidth, ' ');
  std::string hex_column(2 * kTextWidth, ' ');

  // Capture everything (which we use anyway) because:
  //   - clang-tidy complains if you capture a constexpr or const
  //   - MSVC does not automatically capture constexpr types unless we do this
  auto flush = [&] {
    result += text_column;
    result += ' ';
    result += hex_column;
    result += '\n';
    text_column = std::string(kTextWidth, ' ');
    hex_column = std::string(2 * kTextWidth, ' ');
  };

  // Limit the output to the first `max_output_bytes`.
  std::size_t n = size;
  if (max_output_bytes > 0 && max_output_bytes < size) {
    n = max_output_bytes;
  }

  std::size_t count = 0;
  for (char const* c = data; c != data + n; ++c) {
    // std::isprint() actually takes an int argument, signed, without this
    // explicit conversion MSVC in Debug mode asserts an invalid argument, and
    // pops up a nice dialog box that breaks the CI builds.
    int cval = static_cast<unsigned char>(*c);
    if (std::isprint(cval) != 0) {
      text_column[count] = *c;
    } else {
      text_column[count] = '.';
    }
    auto constexpr kCharHexWidth = 2;
    std::array<char, kCharHexWidth + 1> buf{};
    snprintf(buf.data(), buf.size(), "%02x", cval);
    hex_column[2 * count] = buf[0];
    hex_column[2 * count + 1] = buf[1];
    ++count;
    if (count == kTextWidth) {
      flush();
      count = 0;
    }
  }
  if (count != 0) {
    flush();
  }
  return result;
}
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
