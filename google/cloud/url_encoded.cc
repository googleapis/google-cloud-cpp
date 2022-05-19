// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/url_encoded.h"
#include <array>
#include <sstream>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
std::array<const char[4], 95> constexpr kEncode = {
    "%20", "%21", "%22", "%23", "%24", "%25", "%26", "%27", "%28", "%29", "%2A",
    "%2B", "%2C", "-",   ".",   "%2F", "0",   "1",   "2",   "3",   "4",   "5",
    "6",   "7",   "8",   "9",   "%3A", "%3B", "%3C", "%3D", "%3E", "%3F", "%40",
    "A",   "B",   "C",   "D",   "E",   "F",   "G",   "H",   "I",   "J",   "K",
    "L",   "M",   "N",   "O",   "P",   "Q",   "R",   "S",   "T",   "U",   "V",
    "W",   "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "_",   "%60", "a",
    "b",   "c",   "d",   "e",   "f",   "g",   "h",   "i",   "j",   "k",   "l",
    "m",   "n",   "o",   "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "%7B", "%7C", "%7D", "~"};
}  // namespace

std::string UrlEncode(std::string const& input) {
  std::stringstream encoded;
  for (auto const& c : input) {
    encoded << kEncode[c - 0x20];
  }
  return encoded.str();
}

std::string UrlDecode(std::string const& input) {
  std::stringstream decoded;
  for (std::size_t i = 0; i < input.size(); ++i) {
    if (input[i] == '%') {
      decoded << static_cast<char>(
          std::stoi(input.substr(i + 1, 2), nullptr, 16));
      i += 2;
    } else {
      decoded << input[i];
    }
  }
  return decoded.str();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
