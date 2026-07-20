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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CHECKSUM_HELPERS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CHECKSUM_HELPERS_H

#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include "google/cloud/storage/hashing_options.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/options.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

struct HashDisabled {
  bool md5;
  bool crc32c;
};

template <typename Request>
HashDisabled GetDownloadChecksumSettings(Request const& request,
                                         Options const& options) {
  if (request.template HasOption<DisableMD5Hash>() ||
      request.template HasOption<DisableCrc32cChecksum>()) {
    return {
        request.template GetOption<DisableMD5Hash>().value_or(false),
        request.template GetOption<DisableCrc32cChecksum>().value_or(false)};
  }
  if (options.has<DownloadChecksumValidationOption>()) {
    auto const algo = options.get<DownloadChecksumValidationOption>();
    return {algo != ChecksumAlgorithm::kMD5 &&
                algo != ChecksumAlgorithm::kCrc32cAndMD5,
            algo != ChecksumAlgorithm::kCrc32c &&
                algo != ChecksumAlgorithm::kCrc32cAndMD5};
  }
  return {false, false};
}

template <typename Request>
HashDisabled GetUploadChecksumSettings(Request const& request,
                                       Options const& options) {
  if (request.template HasOption<DisableMD5Hash>() ||
      request.template HasOption<DisableCrc32cChecksum>()) {
    return {
        request.template GetOption<DisableMD5Hash>().value_or(false),
        request.template GetOption<DisableCrc32cChecksum>().value_or(false)};
  }
  if (options.has<UploadChecksumValidationOption>()) {
    auto const algo = options.get<UploadChecksumValidationOption>();
    return {algo != ChecksumAlgorithm::kMD5 &&
                algo != ChecksumAlgorithm::kCrc32cAndMD5,
            algo != ChecksumAlgorithm::kCrc32c &&
                algo != ChecksumAlgorithm::kCrc32cAndMD5};
  }
  return {false, false};
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#include "google/cloud/internal/diagnostics_pop.inc"
#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CHECKSUM_HELPERS_H
