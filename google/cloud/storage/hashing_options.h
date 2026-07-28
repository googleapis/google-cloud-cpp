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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_HASHING_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_HASHING_OPTIONS_H

#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include "google/cloud/storage/internal/complex_option.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/version.h"
#include "absl/strings/string_view.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
/**
 * Provide a pre-computed MD5 hash value.
 *
 * The application may be able to obtain a MD5 hash in some out-of-band way.
 * For example, if the object was downloaded from some other cloud storage
 * service, or because the application already queried the GCS object metadata.
 * In these cases, providing the value to the client library improves the
 * end-to-end data integrity verification.
 *
 * @see
 * https://sigops.org/s/conferences/hotos/2021/papers/hotos21-s01-hochschild.pdf
 */
struct MD5HashValue
    : public internal::ComplexOption<MD5HashValue, std::string> {
  using ComplexOption<MD5HashValue, std::string>::ComplexOption;
  // GCC <= 7.0 does not use the inherited default constructor, redeclare it
  // explicitly
  MD5HashValue() = default;
  static char const* name() { return "md5-hash-value"; }
};

/**
 * Compute the MD5 Hash of a buffer in the format preferred by GCS.
 */
std::string ComputeMD5Hash(absl::string_view payload);

/// @copydoc ComputeMD5Hash(absl::string_view)
std::string ComputeMD5Hash(std::string const& payload);

/// @copydoc ComputeMD5Hash(absl::string_view)
inline std::string ComputeMD5Hash(char const* payload) {
  return ComputeMD5Hash(payload == nullptr ? absl::string_view{}
                                           : absl::string_view{payload});
}

/**
 * Provide a pre-computed CRC32C checksum value.
 *
 * The application may be able to obtain a CRC32C checksum in some out-of-band
 * way.  For example, if the object was downloaded from some other cloud storage
 * service, or because the application already queried the GCS object metadata.
 * In these cases, providing the value to the client library improves the
 * end-to-end data integrity verification.
 *
 * @see
 * https://sigops.org/s/conferences/hotos/2021/papers/hotos21-s01-hochschild.pdf
 */
struct Crc32cChecksumValue
    : public internal::ComplexOption<Crc32cChecksumValue, std::string> {
  using ComplexOption<Crc32cChecksumValue, std::string>::ComplexOption;
  // GCC <= 7.0 does not use the inherited default constructor, redeclare it
  // explicitly
  Crc32cChecksumValue() = default;
  static char const* name() { return "crc32c-checksum"; }
};

/**
 * Compute the CRC32C checksum of a buffer in the format preferred by GCS.
 */
std::string ComputeCrc32cChecksum(absl::string_view payload);

/// @copydoc ComputeCrc32cChecksum(absl::string_view payload)
std::string ComputeCrc32cChecksum(std::string const& payload);

/// @copydoc ComputeCrc32cChecksum(absl::string_view payload)
inline std::string ComputeCrc32cChecksum(char const* payload) {
  return ComputeCrc32cChecksum(payload == nullptr ? absl::string_view{}
                                                  : absl::string_view{payload});
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#include "google/cloud/internal/diagnostics_pop.inc"
#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_HASHING_OPTIONS_H
