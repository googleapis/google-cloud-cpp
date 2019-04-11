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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_HASHING_OPTIONS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_HASHING_OPTIONS_H_

#include "google/cloud/storage/internal/complex_option.h"
#include "google/cloud/storage/version.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Provide a pre-computed MD5 hash value to an upload or download request.
 *
 * The application may know what the MD5 hash value of a particular object is,
 * for example, because it was downloaded from GCS or some other cloud storage
 * service. In these cases, the client can include the hash value in the
 * request for better end-to-end validation.
 */
struct MD5HashValue
    : public internal::ComplexOption<MD5HashValue, std::string> {
  using ComplexOption<MD5HashValue, std::string>::ComplexOption;
  static char const* name() { return "md5-hash-value"; }
};

/**
 * Compute the MD5 Hash of a string in the format preferred by GCS.
 */
std::string ComputeMD5Hash(std::string const& payload);

/**
 * Disable MD5 Hashing computations.
 *
 * By default the GCS client library computes MD5 hashes in all
 * `Client::InsertObject` calls. The application can disable the hash
 * computation by passing this parameter.
 *
 * @warning Disabling MD5 hashing exposes your application to data corruption.
 *   We recommend that all uploads to GCS are protected by the supported
 *   checksums and/or hashes.
 */
struct DisableMD5Hash : public internal::ComplexOption<DisableMD5Hash, bool> {
  using ComplexOption<DisableMD5Hash, bool>::ComplexOption;
  static char const* name() { return "disable-md5-hash"; }
};

/**
 * Provide a pre-computed MD5 hash value to an upload or download request.
 *
 * The application may know what the MD5 hash value of a particular object is,
 * for example, because it was downloaded from GCS or some other cloud storage
 * service. In these cases, the client can include the hash value in the
 * request for better end-to-end validation.
 */
struct Crc32cChecksumValue
    : public internal::ComplexOption<Crc32cChecksumValue, std::string> {
  using ComplexOption<Crc32cChecksumValue, std::string>::ComplexOption;
  static char const* name() { return "crc32c-checksum"; }
};

/**
 * Compute the MD5 Hash of a string in the format preferred by GCS.
 */
std::string ComputeCrc32cChecksum(std::string const& payload);

/**
 * Disable MD5 Hashing computations.
 *
 * By default the GCS client library computes MD5 hashes in all
 * `Client::InsertObject` calls. The application can disable the hash
 * computation by passing this parameter.
 *
 * @warning Disabling MD5 hashing exposes your application to data corruption.
 *   We recommend that all uploads to GCS are protected by the supported
 *   checksums and/or hashes.
 */
struct DisableCrc32cChecksum
    : public internal::ComplexOption<DisableCrc32cChecksum, bool> {
  using ComplexOption<DisableCrc32cChecksum, bool>::ComplexOption;
  static char const* name() { return "disable-crc32c-checksum"; }
};

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_HASHING_OPTIONS_H_
