// Copyright 2018 Google LLC
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

#include "google/cloud/storage/hashing_options.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/internal/big_endian.h"
#include <crc32c/crc32c.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string ComputeMD5Hash(absl::string_view payload) {
  return internal::Base64Encode(internal::MD5Hash(payload));
}

std::string ComputeMD5Hash(std::string const& payload) {
  return ComputeMD5Hash(absl::string_view(payload));
}

std::string ComputeCrc32cChecksum(absl::string_view payload) {
  auto checksum = crc32c::Extend(
      0, reinterpret_cast<std::uint8_t const*>(payload.data()), payload.size());
  std::string const hash = google::cloud::internal::EncodeBigEndian(checksum);
  return internal::Base64Encode(hash);
}

std::string ComputeCrc32cChecksum(std::string const& payload) {
  return ComputeCrc32cChecksum(absl::string_view(payload));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
