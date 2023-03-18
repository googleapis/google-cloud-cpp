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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CRC32C_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CRC32C_H

#include "google/cloud/storage/internal/const_buffer.h"
#include "google/cloud/storage/version.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include <cstdint>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::uint32_t ExtendCrc32c(std::uint32_t crc, absl::string_view data);
std::uint32_t ExtendCrc32c(std::uint32_t crc,
                           storage::internal::ConstBufferSequence const& data);
std::uint32_t ExtendCrc32c(std::uint32_t crc, absl::Cord const& data);

std::uint32_t ExtendCrc32c(std::uint32_t crc, absl::string_view data,
                           std::uint32_t data_crc);
std::uint32_t ExtendCrc32c(std::uint32_t crc,
                           storage::internal::ConstBufferSequence const& data,
                           std::uint32_t data_crc);
std::uint32_t ExtendCrc32c(std::uint32_t crc, absl::Cord const& data,
                           std::uint32_t data_crc);

inline std::uint32_t Crc32c(absl::string_view data) {
  return ExtendCrc32c(0, data);
}

inline std::uint32_t Crc32c(
    storage::internal::ConstBufferSequence const& data) {
  return ExtendCrc32c(0, data);
}

inline std::uint32_t Crc32c(absl::Cord const& data) {
  return ExtendCrc32c(0, data);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CRC32C_H
