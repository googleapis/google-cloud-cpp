// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_VALUES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_VALUES_H

#include "google/cloud/storage/version.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

/**
 * A structure to hold GCS object hashes.
 *
 * An empty hash value is used to indicate that the particular hash was not
 * computed or not found. We could have used `optional<std::string>`, but valid
 * hashes are never empty, that seemed like overkill.
 */
struct HashValues {
  /// The CRC32C checksum as a Base64-encoded, 32-bit big endian integer.
  std::string crc32c;
  /// The MD5 hash as a Base64-encoded string.
  std::string md5;
};

std::string Format(HashValues const& values);
HashValues Merge(HashValues a, HashValues b);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_VALUES_H
