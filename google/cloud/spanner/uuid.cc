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
#include "absl/strings/str_format.h"
#include "absl/strings/strip.h"

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// Helper function to parse a single hexadecimal block of a UUID.
// A hexadecimal block is a 16-digit hexadecimal number, which is represented
// as 8 bytes.
StatusOr<std::uint64_t> ParseHexBlock(absl::string_view& str,
                                      absl::string_view original_str) {
  constexpr int kMaxUuidNumberOfHexDigits = 32;
  constexpr int kMaxUuidBlockLength = 16;
  static auto const* char_to_hex = new std::unordered_map<char, std::uint8_t>(
      {{'0', 0x00}, {'1', 0x01}, {'2', 0x02}, {'3', 0x03}, {'4', 0x04},
       {'5', 0x05}, {'6', 0x06}, {'7', 0x07}, {'8', 0x08}, {'9', 0x09},
       {'a', 0x0a}, {'b', 0x0b}, {'c', 0x0c}, {'d', 0x0d}, {'e', 0x0e},
       {'f', 0x0f}, {'A', 0x0a}, {'B', 0x0b}, {'C', 0x0c}, {'D', 0x0d},
       {'E', 0x0e}, {'F', 0x0f}});
  std::uint64_t block = 0;
  for (int j = 0; j < kMaxUuidBlockLength; ++j) {
    absl::ConsumePrefix(&str, "-");
    if (str.empty()) {
      return internal::InvalidArgumentError(
          absl::StrFormat("UUID must contain %d hexadecimal digits: %s",
                          kMaxUuidNumberOfHexDigits, original_str),
          GCP_ERROR_INFO());
    }
    auto it = char_to_hex->find(str[0]);
    if (it == char_to_hex->end()) {
      if (str[0] == '-') {
        return internal::InvalidArgumentError(
            absl::StrFormat("UUID cannot contain consecutive hyphens: %s",
                            original_str),
            GCP_ERROR_INFO());
      }

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

Uuid::operator std::string() const {
  constexpr int kUuidStringLen = 36;
  constexpr int kChunkLength[] = {8, 4, 4, 4, 12};
  auto to_hex = [](std::uint64_t v, int start_index, int end_index, char* out) {
    static constexpr char kHexChar[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    for (int i = start_index; i >= end_index; --i) {
      *out++ = kHexChar[(v >> (i * 4)) & 0xf];
    }
    return start_index - end_index + 1;
  };

  std::string output;
  output.resize(kUuidStringLen);
  char* target = const_cast<char*>(output.data());
  char* const last = &((output)[output.size()]);
  auto bits = Uint128High64(uuid_);
  int start = 16;
  for (auto length : kChunkLength) {
    int end = start - length;
    target += to_hex(bits, start - 1, end, target);
    // Only hyphens write to valid addresses.
    if (target < last) *(target++) = '-';
    if (end == 0) {
      start = 16;
      bits = Uint128Low64(uuid_);
    } else {
      start = end;
    }
  }
  return output;
}

StatusOr<Uuid> MakeUuid(absl::string_view str) {
  if (str.empty()) {
    return internal::InvalidArgumentError(
        absl::StrFormat("UUID cannot be empty"), GCP_ERROR_INFO());
  }

  std::string original_str = std::string(str);
  // Check and remove optional braces
  if (absl::ConsumePrefix(&str, "{")) {
    if (!absl::ConsumeSuffix(&str, "}")) {
      return internal::InvalidArgumentError(
          absl::StrFormat("UUID missing closing '}': %s", original_str),
          GCP_ERROR_INFO());
    }
  }

  // Check for leading hyphen after stripping any surrounding braces.
  if (absl::StartsWith(str, "-")) {
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
        absl::StrFormat("Extra characters found after parsing UUID: %s", str),
        GCP_ERROR_INFO());
  }

  return Uuid(absl::MakeUint128(*high_bits, *low_bits));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
