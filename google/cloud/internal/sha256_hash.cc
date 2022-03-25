// Copyright 2019 Google LLC
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

#include "google/cloud/internal/sha256_hash.h"
#include "absl/base/casts.h"
#include <openssl/sha.h>
#include <array>
#include <cassert>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

namespace {
template <typename Byte,
          typename std::enable_if<sizeof(Byte) == 1, int>::type = 0>
std::array<std::uint8_t, 32> Sha256Hash(Byte const* data, std::size_t count) {
  SHA256_CTX sha256;
  SHA256_Init(&sha256);
  SHA256_Update(&sha256, data, count);

  std::array<unsigned char, SHA256_DIGEST_LENGTH> hash{};
  SHA256_Final(hash.data(), &sha256);
  // absl::bit_cast is a backport of std::bit_cast from c++20 that checks that
  // the types are trivially copyable
  // and the same size and converts between them in a way that is not UB.
  return absl::bit_cast<std::array<std::uint8_t, 32>>(hash);
}
}  // namespace

std::array<std::uint8_t, 32> Sha256Hash(std::string const& str) {
  return Sha256Hash(str.data(), str.size());
}

std::array<std::uint8_t, 32> Sha256Hash(
    std::vector<std::uint8_t> const& bytes) {
  return Sha256Hash(bytes.data(), bytes.size());
}

std::string HexEncode(absl::Span<std::uint8_t const> bytes) {
  std::string result;
  std::array<char, sizeof("ff")> buf{};
  for (auto c : bytes) {
    std::snprintf(buf.data(), buf.size(), "%02x", c);
    result += buf.data();
  }
  return result;
}

std::vector<std::uint8_t> HexDecode(std::string const& str) {
  if (str.size() % 2 != 0) return {};

  std::vector<std::uint8_t> result;
  result.reserve(str.size() / 2);
  for (char const* p = str.data(); p != str.data() + str.size(); p += 2) {
    std::string s{p, p + 2};
    std::size_t idx = 0;
    auto constexpr kBaseForHex = 16;
    auto v = std::stol(s, &idx, kBaseForHex);
    if (idx != 2) return {};
    result.push_back(static_cast<std::uint8_t>(v));
  }
  return result;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
