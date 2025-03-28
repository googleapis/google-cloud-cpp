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
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/strip.h"
#include <cctype>
#include <cstring>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

constexpr char kHexDigits[] = "0123456789abcdef";

Uuid::Uuid(std::uint64_t high_bits, std::uint64_t low_bits)
    : Uuid(absl::MakeUint128(high_bits, low_bits)) {}

std::pair<std::uint64_t, std::uint64_t> Uuid::As64BitPair() const {
  return std::make_pair(Uint128High64(uuid_), Uint128Low64(uuid_));
}

Uuid::operator std::string() const {
  constexpr char kTemplate[] = "00000000-0000-0000-0000-000000000000";
  char buf[sizeof kTemplate];
  auto uuid = uuid_;
  for (auto j = sizeof buf; j-- != 0;) {
    if (kTemplate[j] != '0') {
      buf[j] = kTemplate[j];
    } else {
      buf[j] = kHexDigits[static_cast<int>(uuid & 0xf)];
      uuid >>= 4;
    }
  }
  return buf;
}

StatusOr<Uuid> MakeUuid(absl::string_view str) {
  absl::uint128 uuid = 0;
  auto const original_str = str;
  if (absl::StartsWith(str, "{") && absl::ConsumeSuffix(&str, "}")) {
    str.remove_prefix(1);
  }
  if (absl::StartsWithIgnoreCase(str, "0x")) {
    str.remove_prefix(2);
  }
  constexpr int kUuidNumberOfHexDigits = 32;
  for (int j = 0; j != kUuidNumberOfHexDigits; ++j) {
    if (j != 0) absl::ConsumePrefix(&str, "-");
    if (str.empty()) {
      return internal::InvalidArgumentError(
          absl::StrFormat("UUID must contain %v hexadecimal digits: %v",
                          kUuidNumberOfHexDigits, original_str),
          GCP_ERROR_INFO());
    }
    auto const* dp = std::strchr(
        kHexDigits, std::tolower(static_cast<unsigned char>(str[0])));
    if (dp == nullptr) {
      return internal::InvalidArgumentError(
          absl::StrFormat(
              "UUID contains invalid character '%c' at position %v: %v", str[0],
              str.data() - original_str.data(), original_str),
          GCP_ERROR_INFO());
    }
    uuid <<= 4;
    uuid += dp - kHexDigits;
    str.remove_prefix(1);
  }
  if (!str.empty()) {
    return internal::InvalidArgumentError(
        absl::StrFormat("Extra characters \"%v\" found after parsing UUID: %v",
                        str, original_str),
        GCP_ERROR_INFO());
  }
  return Uuid{uuid};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
