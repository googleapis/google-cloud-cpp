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

#include "google/cloud/storage/internal/crc32c.h"
#include "absl/base/config.h"
#if defined(ABSL_LTS_RELEASE_VERSION) && ABSL_LTS_RELEASE_VERSION >= 20230125
#include "absl/crc/crc32c.h"
#define GOOGLE_CLOUD_CPP_USE_ABSL_CRC32C 1
#else
#include <crc32c/crc32c.h>
#define GOOGLE_CLOUD_CPP_USE_ABSL_CRC32C 0
#endif  // ABSL_LTS_RELEASE_VERSION

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::uint32_t ExtendCrc32c(std::uint32_t crc,
                           storage::internal::ConstBufferSequence const& data) {
  for (auto const& b : data) {
    crc = ExtendCrc32c(crc, absl::string_view{b.data(), b.size()});
  }
  return crc;
}

std::uint32_t ExtendCrc32c(std::uint32_t crc, absl::Cord const& data) {
  for (auto i = data.chunk_begin(); i != data.chunk_end(); ++i) {
    crc = ExtendCrc32c(crc, *i);
  }
  return crc;
}

#if GOOGLE_CLOUD_CPP_USE_ABSL_CRC32C

std::uint32_t ExtendCrc32c(std::uint32_t crc, absl::string_view data) {
  return static_cast<std::uint32_t>(absl::ExtendCrc32c(absl::crc32c_t{crc}, data));
}

std::uint32_t ExtendCrc32c(std::uint32_t crc, absl::string_view data,
                           std::uint32_t data_crc) {
  return static_cast<std::uint32_t>(absl::ConcatCrc32c(
      absl::crc32c_t{crc}, absl::crc32c_t{data_crc}, data.size()));
}

std::uint32_t ExtendCrc32c(std::uint32_t crc,
                           storage::internal::ConstBufferSequence const& data,
                           std::uint32_t data_crc) {
  auto const size = storage::internal::TotalBytes(data);
  return static_cast<std::uint32_t>(
      absl::ConcatCrc32c(absl::crc32c_t{crc}, absl::crc32c_t{data_crc}, size));
}

std::uint32_t ExtendCrc32c(std::uint32_t crc, absl::Cord const& data,
                           std::uint32_t data_crc) {
  return static_cast<std::uint32_t>(absl::ConcatCrc32c(
      absl::crc32c_t{crc}, absl::crc32c_t{data_crc}, data.size()));
}

#else

std::uint32_t ExtendCrc32c(std::uint32_t crc, absl::string_view data) {
  return crc32c::Extend(crc, reinterpret_cast<uint8_t const*>(data.data()),
                        data.size());
}

std::uint32_t ExtendCrc32c(std::uint32_t crc, absl::string_view data,
                           std::uint32_t /*data_crc*/) {
  return ExtendCrc32c(crc, data);
}

std::uint32_t ExtendCrc32c(std::uint32_t crc,
                           storage::internal::ConstBufferSequence const& data,
                           std::uint32_t /*data_crc*/) {
  return ExtendCrc32c(crc, data);
}

std::uint32_t ExtendCrc32c(std::uint32_t crc, absl::Cord const& data,
                           std::uint32_t /*data_crc*/) {
  return ExtendCrc32c(crc, data);
}

#endif  // GOOGLE_CLOUD_CPP_USE_ABSL_CRC32C

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
