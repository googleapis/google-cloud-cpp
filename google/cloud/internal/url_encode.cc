// Copyright 2023 Google LLC
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

#include "google/cloud/internal/url_encode.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

namespace {

std::string ToHex(int i) {
  std::stringstream stream;
  stream << "%" << std::setfill('0') << std::setw(2) << std::uppercase
         << std::hex << i;
  return stream.str();
}

std::string Escape(char c) {
  static auto const* const kDict = []() {
    static std::unordered_map<char, std::string> m;
    for (auto i = 0; i < 32; ++i) {
      m[static_cast<char>(i)] = ToHex(i);
    }
    for (auto c : {
             ' ', '\"', '#', '$', '%', '&',  '+', ',', '/', ':', ';', '<',
             '=', '>',  '?', '@', '[', '\\', ']', '^', '`', '{', '|', '}',
         }) {
      m[c] = ToHex(static_cast<int>(c));
    }
    for (auto i = 127; i < 256; ++i) {
      m[static_cast<char>(i)] = ToHex(i);
    }
    return &m;
  }();
  auto it = kDict->find(c);
  if (it != kDict->end()) return it->second;
  return std::string{c};
}

// Returns 0-15 if c is in [0-9A-Fa-f], and -1 otherwise.
int ParseDigitHex(char c) {
  if ('0' <= c && c <= '9') return c - '0';
  if ('A' <= c && c <= 'F') return c - 'A' + 0xA;
  if ('a' <= c && c <= 'f') return c - 'a' + 0xa;
  return -1;
}

}  // namespace

std::string UrlEncode(absl::string_view value) {
  std::string s;
  for (auto c : value) {
    absl::StrAppend(&s, Escape(c));
  }
  return s;
}

std::string UrlDecode(absl::string_view value) {
  std::string s;
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '%' && i < value.size() - 2) {
      auto b1 = ParseDigitHex(value[i + 1]);
      auto b0 = ParseDigitHex(value[i + 2]);
      if (b1 != -1 && b0 != -1) {
        s += static_cast<char>((b1 << 4) + b0);
        i += 2;
        continue;
      }
    }
    s += value[i];
  }
  return s;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
