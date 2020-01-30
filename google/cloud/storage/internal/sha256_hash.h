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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SHA256_HASH_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SHA256_HASH_H

#include "google/cloud/storage/version.h"
#include <cstdint>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

/// Return the SHA256 hash (as raw bytes) of @p str.
std::vector<std::uint8_t> Sha256Hash(std::string const& str);

/// Return the SHA256 hash (as raw bytes) of @p bytes.
std::vector<std::uint8_t> Sha256Hash(std::vector<std::uint8_t> const& bytes);

/// Return @p bytes encoded as a lowercase hexadecimal string.
std::string HexEncode(std::vector<std::uint8_t> const& bytes);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SHA256_HASH_H
