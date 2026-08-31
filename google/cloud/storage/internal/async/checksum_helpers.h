// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_CHECKSUM_HELPERS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_CHECKSUM_HELPERS_H

#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include "google/cloud/storage/async/options.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/options.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct ChecksumSettings {
  bool enable_crc32c;
  bool enable_md5;
};

inline ChecksumSettings GetDownloadChecksumSettings(Options const& options) {
  if (options.has<storage::DownloadChecksumValidationOption>()) {
    auto const algo = options.get<storage::DownloadChecksumValidationOption>();
    return {algo == storage::ChecksumAlgorithm::kCrc32c,
            algo == storage::ChecksumAlgorithm::kMD5};
  }
  return {options.get<storage::EnableCrc32cValidationOption>(),
          options.get<storage::EnableMD5ValidationOption>()};
}

inline ChecksumSettings GetUploadChecksumSettings(Options const& options) {
  if (options.has<storage::UploadChecksumValidationOption>()) {
    auto const algo = options.get<storage::UploadChecksumValidationOption>();
    return {algo == storage::ChecksumAlgorithm::kCrc32c,
            algo == storage::ChecksumAlgorithm::kMD5};
  }
  return {options.get<storage::EnableCrc32cValidationOption>(),
          options.get<storage::EnableMD5ValidationOption>()};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#include "google/cloud/internal/diagnostics_pop.inc"
#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_CHECKSUM_HELPERS_H
