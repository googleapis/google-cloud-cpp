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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SHA256_HASH_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SHA256_HASH_H

#include "google/cloud/version.h"
#include "absl/types/span.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

// (8 bits per byte) * 32 bytes = 256 bits
using Sha256Type = std::array<std::uint8_t, 32>;

/// Return the SHA256 hash (as raw bytes) of @p str.
Sha256Type Sha256Hash(std::string const& str);

/// Return the SHA256 hash (as raw bytes) of @p bytes.
Sha256Type Sha256Hash(std::vector<std::uint8_t> const& bytes);

/// Return @p bytes encoded as a lowercase hexadecimal string.
std::string HexEncode(absl::Span<std::uint8_t const> bytes);

/// Parse @p str as a hex-encoded string.
std::vector<std::uint8_t> HexDecode(std::string const& str);

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SHA256_HASH_H
