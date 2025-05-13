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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_OPTIONS_H

#include "google/cloud/version.h"
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * If enabled, the client computes (if necessary) and validates the CRC32C
 * checksum of an object during uploads and downloads.
 *
 * The option has no effect if the service does not return a CRC32C hash.
 */
struct EnableCrc32cValidationOption {
  using Type = bool;
};

/**
 * If this option is present, the client does not compute the MD5 hash of an
 * object during uploads or downloads.
 *
 * Using a known value of the CRC32C checksum can reduce the computation
 * overhead of an object upload or download. It also provides better end to end
 * integrity when the object data is obtained from a source that provides such
 * checksums. Note that this option has no effect if the service does not return
 * or compute a CRC32C checksum.
 */
struct UseCrc32cValueOption {
  using Type = std::uint32_t;
};

/**
 * If enabled, the client computes (if necessary) and validates the MD5 hash
 * of an object during uploads and downloads.
 *
 * The option has no effect for partial downloads or any other circumstance
 * where the service does not return a MD5 hash.
 */
struct EnableMD5ValidationOption {
  using Type = bool;
};

/**
 * If this option is present, the client does not compute the MD5 hash of an
 * object during uploads or downloads.
 *
 * Using a known value of the MD5 hash can reduce the computation overhead of
 * an object upload or download. It also provides better end to end integrity
 * when the object data is obtained from a source that provides such checksums.
 * Note that this option has no effect if the service does not return or compute
 * a MD5 hash.
 */
struct UseMD5ValueOption {
  using Type = std::string;
};

/**
 * Use this option to limit the size of `ObjectDescriptor::Read()` requests.
 *
 * @par Example
 * @code
 * auto client = gcs::AsyncClient(gcs::MakeAsyncConnection(
 *   Options{}.set<MaximumRangeSizeOption>(128L * 1024 * 1024)));
 * @endcode
 */
struct MaximumRangeSizeOption {
  using Type = std::uint64_t;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_OPTIONS_H
