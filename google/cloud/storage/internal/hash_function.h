// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_FUNCTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_FUNCTION_H

#include "google/cloud/storage/hashing_options.h"
#include "google/cloud/storage/internal/hash_values.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

class ReadObjectRangeRequest;
class ResumableUploadRequest;

/**
 * Defines the interface to compute hash values during uploads and downloads.
 *
 * To verify data integrity GCS offers both MD5 and CRC32C hashes (though the
 * latter are referred to as "checksums"). The library needs to compute these
 * hashes to:
 * - Detect data corruption on downloads, by comparing the hashes reported by
 *   the service against the computed values.
 * - Detect data corruption on uploads, by including the hashes with the upload,
 *   so the service can detect any differences.
 *
 * There are a number of caveats, as usual:
 *
 * - MD5 hashes are expensive to compute, and sometimes the service omits them.
 *   The library does not compute them unless explicitly asked by the
 *   application.
 * - The application may have computed the hashes already, or may have received
 *   them from the original source of the data, in which case we don't want to
 *   compute any hashes.
 * - With gRPC the hashes can be included at the end of an upload, but with REST
 *   we can only (a) read the data twice and include the hashes at the start
 *   of the upload, or (b) compare the computed hashes against the values
 *   reported by the service when the upload completes.
 *
 * This suggests a design that must support computing no hashes, one of the two
 * hash functions, or both hash functions. We use the [Composite] and
 * [Null Object] patterns to satisfy these requirements.
 *
 * [Composite]: https://en.wikipedia.org/wiki/Composite_pattern
 * [Null Object]: https://en.wikipedia.org/wiki/Null_object_pattern
 */
class HashFunction {
 public:
  virtual ~HashFunction() = default;

  /// A short string that names the function when composing results.
  virtual std::string Name() const = 0;

  /// Update the computed hash value with some portion of the data.
  virtual void Update(absl::string_view buffer) = 0;
  virtual Status Update(std::int64_t offset, absl::string_view buffer) = 0;
  virtual Status Update(std::int64_t offset, absl::string_view buffer,
                        std::uint32_t buffer_crc) = 0;
  virtual Status Update(std::int64_t offset, absl::Cord const& buffer,
                        std::uint32_t buffer_crc) = 0;

  /**
   * Compute the final hash values.
   */
  virtual HashValues Finish() = 0;
};

/// Create a hash function configured by several options.
std::unique_ptr<HashFunction> CreateHashFunction(
    Crc32cChecksumValue const& crc32c_value,
    DisableCrc32cChecksum const& crc32c_disabled, MD5HashValue const& md5_value,
    DisableMD5Hash const& md5_disabled);

/// Create a no-op hash function
std::unique_ptr<HashFunction> CreateNullHashFunction();

/// Create a hash function configured by @p request.
std::unique_ptr<HashFunction> CreateHashFunction(
    ReadObjectRangeRequest const& request);

/// Create a hash function configured by @p request.
std::unique_ptr<HashFunction> CreateHashFunction(
    ResumableUploadRequest const& request);

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HASH_FUNCTION_H
