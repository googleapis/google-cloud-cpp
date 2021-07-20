// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_HASHING_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_HASHING_OPTIONS_H

#include "google/cloud/storage/internal/complex_option.h"
#include "google/cloud/storage/version.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
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
 * Compute the MD5 Hash of a string in the format preferred by GCS.
 */
std::string ComputeMD5Hash(std::string const& payload);

/**
 * Disable or enable MD5 Hashing computations.
 *
 * By default MD5 hashes are disabled. To enable them use the
 * `EnableMD5Hash()` helper function.
 *
 * @warning MD5 hashes are disabled by default, as they are computationally
 *   expensive, and CRC32C checksums provide enough data integrity protection
 *   for most applications.  Disabling CRC32C checksums while MD5 hashes remain
 *   disabled exposes your application to data corruption. We recommend that all
 *   uploads to GCS and downloads from GCS use CRC32C checksums.
 */
struct DisableMD5Hash : public internal::ComplexOption<DisableMD5Hash, bool> {
  using ComplexOption<DisableMD5Hash, bool>::ComplexOption;
  // GCC <= 7.0 does not use the inherited default constructor, redeclare it
  // explicitly
  DisableMD5Hash() : DisableMD5Hash(true) {}
  static char const* name() { return "disable-md5-hash"; }
};

/**
 * Enable MD5 hashes in upload and download operations.
 *
 * Use this function where the option `DisableMD5Hash` is expected to enable MD5
 * hashes.
 */
inline DisableMD5Hash EnableMD5Hash() { return DisableMD5Hash(false); }

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
 * Compute the CRC32C checksum of a string in the format preferred by GCS.
 */
std::string ComputeCrc32cChecksum(std::string const& payload);

/**
 * Disable CRC32C checksum computations.
 *
 * By default the GCS client library computes CRC32C checksums in all upload and
 * download operations. The application can use this option to disable the
 * checksum computation.
 *
 * @warning MD5 hashes are disabled by default, as they are computationally
 *   expensive, and CRC32C checksums provide enough data integrity protection
 *   for most applications.  Disabling CRC32C checksums while MD5 hashes remain
 *   disabled exposes your application to data corruption. We recommend that all
 *   uploads to GCS and downloads from GCS use CRC32C checksums.
 */
struct DisableCrc32cChecksum
    : public internal::ComplexOption<DisableCrc32cChecksum, bool> {
  using ComplexOption<DisableCrc32cChecksum, bool>::ComplexOption;
  // GCC <= 7.0 does not use the inherited default constructor, redeclare it
  // explicitly
  DisableCrc32cChecksum() = default;
  static char const* name() { return "disable-crc32c-checksum"; }
};

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_HASHING_OPTIONS_H
