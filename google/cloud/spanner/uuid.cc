// Copyright 2025 Google LLC
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

#include "google/cloud/spanner/uuid.h"
#include "google/cloud/internal/make_status.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/strip.h"

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

constexpr int kMaxUuidNumberOfHexDigits = 32;

// Helper function to parse a single hexadecimal block of a UUID.
// A hexadecimal block is a 16-digit hexadecimal number, which is represented
// as 8 bytes.
StatusOr<uint64_t> ParseHexBlock(absl::string_view& str,
                                 absl::string_view original_str) {
  constexpr int kMaxUuidBlockLength = 16;
  static auto const* char_to_hex = new absl::flat_hash_map<char, std::uint8_t>(
      {{'0', 0x00}, {'1', 0x01}, {'2', 0x02}, {'3', 0x03}, {'4', 0x04},
       {'5', 0x05}, {'6', 0x06}, {'7', 0x07}, {'8', 0x08}, {'9', 0x09},
       {'a', 0x0a}, {'b', 0x0b}, {'c', 0x0c}, {'d', 0x0d}, {'e', 0x0e},
       {'f', 0x0f}, {'A', 0x0a}, {'B', 0x0b}, {'C', 0x0c}, {'D', 0x0d},
       {'E', 0x0e}, {'F', 0x0f}});
  uint64_t block = 0;
  for (int j = 0; j < kMaxUuidBlockLength; ++j) {
    absl::ConsumePrefix(&str, "-");
    if (str.empty()) {
      return internal::InvalidArgumentError(
          absl::StrFormat("UUID must be at least %d characters long: %s",
                          kMaxUuidNumberOfHexDigits, original_str),
          GCP_ERROR_INFO());
    }
    if (str[0] == '-') {
      return internal::InvalidArgumentError(
          absl::StrFormat("UUID cannot contain consecutive hyphens: %s",
                          original_str),
          GCP_ERROR_INFO());
    }

    auto it = char_to_hex->find(str[0]);
    if (it == char_to_hex->end()) {
      return internal::InvalidArgumentError(
          absl::StrFormat("UUID contains invalid character (%c): %s", str[0],
                          original_str),
          GCP_ERROR_INFO());
    }
    block = (block << 4) + it->second;
    str.remove_prefix(1);
  }
  return block;
}
}  // namespace

Uuid::Uuid(absl::uint128 value) : uuid_(value) {}

Uuid::Uuid(std::uint64_t high_bits, std::uint64_t low_bits)
    : Uuid(absl::MakeUint128(high_bits, low_bits)) {}

bool operator==(Uuid const& lhs, Uuid const& rhs) {
  return lhs.uuid_ == rhs.uuid_;
}

bool operator<(Uuid const& lhs, Uuid const& rhs) {
  return lhs.uuid_ < rhs.uuid_;
}

Uuid::operator std::string() const {
  constexpr int kUuidStringLen = 36;
  constexpr int kHyphenPos[] = {8, 13, 18, 23};
  auto to_hex = [](std::uint64_t v, int start_index, int end_index, char* out) {
    static constexpr char kHexChar[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    for (int i = start_index; i >= end_index; --i) {
      *out++ = kHexChar[(v >> (i * 4)) & 0xf];
    }
  };

  std::string output;
  output.resize(kUuidStringLen);
  char* target = &((output)[output.size() - kUuidStringLen]);
  auto high_bits = Uint128High64(uuid_);
  auto low_bits = Uint128Low64(uuid_);
  to_hex(high_bits, 15, 8, target);
  *(target + kHyphenPos[0]) = '-';
  to_hex(high_bits, 7, 4, target + kHyphenPos[0] + 1);
  *(target + kHyphenPos[1]) = '-';
  to_hex(high_bits, 3, 0, target + kHyphenPos[1] + 1);
  *(target + kHyphenPos[2]) = '-';
  to_hex(low_bits, 15, 12, target + kHyphenPos[2] + 1);
  *(target + kHyphenPos[3]) = '-';
  to_hex(low_bits, 11, 0, target + kHyphenPos[3] + 1);

  return output;
}

StatusOr<Uuid> MakeUuid(absl::string_view str) {
  // Early checks for invalid length or leading hyphen
  if (str.size() < kMaxUuidNumberOfHexDigits) {
    return internal::InvalidArgumentError(
        absl::StrFormat("UUID must be at least %d characters long: %s",
                        kMaxUuidNumberOfHexDigits, str),
        GCP_ERROR_INFO());
  }
  std::string original_str = std::string(str);
  // Check and remove optional braces
  bool has_braces = (str[0] == '{');
  if (has_braces) {
    if (str[str.size() - 1] != '}') {
      return internal::InvalidArgumentError(
          absl::StrFormat("UUID missing closing '}': %s", original_str),
          GCP_ERROR_INFO());
    }
    str.remove_prefix(1);
    str.remove_suffix(1);
  }

  // Check for leading hyphen after braces.
  if (str[0] == '-') {
    return internal::InvalidArgumentError(
        absl::StrFormat("UUID cannot begin with '-': %s", original_str),
        GCP_ERROR_INFO());
  }

  auto high_bits = ParseHexBlock(str, original_str);
  if (!high_bits.ok()) return std::move(high_bits).status();
  auto low_bits = ParseHexBlock(str, original_str);
  if (!low_bits.ok()) return std::move(low_bits).status();

  if (!str.empty()) {
    return internal::InvalidArgumentError(
        absl::StrFormat("Extra characters (%d) found after parsing UUID: %s",
                        str.size(), original_str),
        GCP_ERROR_INFO());
  }

  return Uuid(absl::MakeUint128(*high_bits, *low_bits));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
