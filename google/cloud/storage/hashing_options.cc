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
#include "google/cloud/storage/internal/base64.h"
#include "google/cloud/storage/internal/crc32c.h"
#include "google/cloud/storage/internal/md5hash.h"
#include "google/cloud/internal/big_endian.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string ComputeMD5Hash(absl::string_view payload) {
  return internal::Base64Encode(storage_internal::MD5Hash(payload));
}

std::string ComputeMD5Hash(std::string const& payload) {
  return ComputeMD5Hash(absl::string_view(payload));
}

std::string ComputeCrc32cChecksum(absl::string_view payload) {
  auto checksum = storage_internal::Crc32c(payload);
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
