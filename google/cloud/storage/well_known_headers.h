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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_WELL_KNOWN_HEADERS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_WELL_KNOWN_HEADERS_H

#include "google/cloud/storage/version.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/optional.h"
#include "absl/types/optional.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Defines well-known request headers using the CRTP.
 *
 * @tparam H the type we will use to represent the header.
 * @tparam T the C++ type of the query parameter
 */
template <typename H, typename T>
class WellKnownHeader {
 public:
  WellKnownHeader() : value_{} {}
  explicit WellKnownHeader(T value) : value_(std::move(value)) {}

  char const* header_name() const { return H::header_name(); }
  bool has_value() const { return value_.has_value(); }
  T const& value() const { return value_.value(); }
  template <typename U>
  T value_or(U&& default_val) {
    return value_.value_or(std::forward<U>(default_val));
  }

 private:
  absl::optional<T> value_;
};

template <typename H, typename T>
std::ostream& operator<<(std::ostream& os, WellKnownHeader<H, T> const& rhs) {
  if (rhs.has_value()) {
    return os << rhs.header_name() << ": " << rhs.value();
  }
  return os << rhs.header_name() << ": <not set>";
}
}  // namespace internal

/**
 * Set the MIME content type of an object.
 *
 * This optional parameter sets the content-type of an object during uploads,
 * without having to configure all the other metadata attributes.
 */
struct ContentType
    : public internal::WellKnownHeader<ContentType, std::string> {
  using WellKnownHeader<ContentType, std::string>::WellKnownHeader;
  static char const* header_name() { return "content-type"; }
};

/**
 * An option to inject custom headers into the request.
 *
 * In some cases it is necessary to inject a custom header into the request. For
 * example, because the protocol has added new headers and the library has not
 * been updated to support them, or because
 */
class CustomHeader
    : public internal::WellKnownHeader<CustomHeader, std::string> {
 public:
  CustomHeader() = default;
  explicit CustomHeader(std::string name, std::string value)
      : WellKnownHeader(std::move(value)), name_(std::move(name)) {}

  std::string const& custom_header_name() const { return name_; }

 private:
  std::string name_;
};

std::ostream& operator<<(std::ostream& os, CustomHeader const& rhs);

/**
 * A pre-condition: apply this operation only if the HTTP Entity Tag matches.
 *
 * [HTTP Entity Tags](https://en.wikipedia.org/wiki/HTTP_ETag) allow
 * applications to conditionally execute a query only if the target resource
 * matches the expected state. This can be useful, for example, to implement
 * optimistic concurrency control in the application.
 */
struct IfMatchEtag
    : public internal::WellKnownHeader<IfMatchEtag, std::string> {
  using WellKnownHeader<IfMatchEtag, std::string>::WellKnownHeader;
  static char const* header_name() { return "If-Match"; }
};

/**
 * A pre-condition: apply this operation only if the HTTP Entity Tag does not
 * match.
 *
 * [HTTP Entity Tags](https://en.wikipedia.org/wiki/HTTP_ETag) allow
 * applications to conditionally execute a query only if the target resource
 * matches the expected state. This can be useful, for example, to implement
 * optimistic concurrency control in the application.
 */
struct IfNoneMatchEtag
    : public internal::WellKnownHeader<IfNoneMatchEtag, std::string> {
  using WellKnownHeader<IfNoneMatchEtag, std::string>::WellKnownHeader;
  static char const* header_name() { return "If-None-Match"; }
};

/**
 * A simple wrapper for the encryption key attributes.
 *
 * Most request options have primitive types such as integers or strings.
 * Encryption keys, in contrast, must include the algorithm, the
 * (base64-encoded) key, and the (base64-encoded) hash of the key. This
 * structure provides a simple container for these three values.
 */
struct EncryptionKeyData {
  std::string algorithm;
  std::string key;
  std::string sha256;
};

/**
 * Formats a (potentially binary) encryption key in the format required by the
 * Google Cloud Storage API.
 *
 * @param key a binary key, must have exactly 32 bytes.
 */
EncryptionKeyData EncryptionDataFromBinaryKey(std::string const& key);

/**
 * Formats an encryption key in base64 format to the data structure required by
 * the Google Cloud Storage API.
 *
 * @param key a base64-encoded key, must have exactly 32 bytes when decoded.
 */
EncryptionKeyData EncryptionDataFromBase64Key(std::string const& key);

/**
 * An optional parameter to set the Customer-Supplied Encryption key.
 *
 * Application developers can generate their own encryption keys to protect the
 * data in GCS. This is known as a Customer-Supplied Encryption key (CSEK). If
 * the application provides a CSEK, GCS does not retain the key. The object
 * data, the object CRC32 checksum, and its MD5 hash (if applicable) are all
 * encrypted with this key, and the key is required to read any of these
 * elements back.
 *
 * Care must be taken to save and protect these keys, if lost, the data is not
 * recoverable.  Also, applications should avoid generating predictable keys,
 * as this weakens the encryption.
 *
 * This option is used in read (download), write (upload), copy, and compose
 * operations. Note that copy and compose operations use the same key for the
 * source and destination objects.
 *
 * @see https://cloud.google.com/storage/docs/encryption/customer-supplied-keys
 *     for a detailed description of how Customer Supplied Encryption keys are
 *     used in GCS.
 */
struct EncryptionKey
    : public internal::WellKnownHeader<EncryptionKey, EncryptionKeyData> {
  using WellKnownHeader<EncryptionKey, EncryptionKeyData>::WellKnownHeader;

  /**
   * Create an encryption key parameter from a binary key.
   *
   * @param key a binary key, must have exactly 32 bytes.
   */
  static EncryptionKey FromBinaryKey(std::string const& key);

  /**
   * Creates an encryption key parameter from a key in base64 format.
   *
   * @param key a base64-encoded key, must have exactly 32 bytes when decoded.
   */
  static EncryptionKey FromBase64Key(std::string const& key);

  static char const* prefix() { return "x-goog-encryption-"; }
};

std::ostream& operator<<(std::ostream& os, EncryptionKey const& rhs);

/**
 * An optional parameter to set the Customer-Supplied Encryption key for rewrite
 * source object.
 *
 * Application developers can generate their own encryption keys to protect the
 * data in GCS. This is known as a Customer-Supplied Encryption key (CSEK). If
 * the application provides a CSEK, GCS does not retain the key. The object
 * data, the object CRC32 checksum, and its MD5 hash (if applicable) are all
 * encrypted with this key, and the key is required to read any of these
 * elements back.
 *
 * Care must be taken to save and protect these keys, if lost, the data is not
 * recoverable.  Also, applications should avoid generating predictable keys,
 * as this weakens the encryption.
 *
 * This option is used only in rewrite operations and it defines the key used
 * for the source object.
 *
 * @see https://cloud.google.com/storage/docs/encryption/customer-supplied-keys
 *     for a detailed description of how Customer Supplied Encryption keys are
 *     used in GCS.
 */
struct SourceEncryptionKey
    : public internal::WellKnownHeader<SourceEncryptionKey, EncryptionKeyData> {
  using WellKnownHeader<SourceEncryptionKey,
                        EncryptionKeyData>::WellKnownHeader;

  /**
   * Creates a source encryption key parameter from a binary key.
   *
   * @param key a binary key, must have exactly 32 bytes.
   */
  static SourceEncryptionKey FromBinaryKey(std::string const& key);

  /**
   * Creates an encryption key parameter from a key in base64 format.
   *
   * @param key a base64-encoded key, must have exactly 32 bytes when decoded.
   */
  static SourceEncryptionKey FromBase64Key(std::string const& key);

  static char const* prefix() { return "x-goog-copy-source-encryption-"; }
};

std::ostream& operator<<(std::ostream& os, SourceEncryptionKey const& rhs);

/**
 * Creates an encryption key parameter from a pseudo-random number generator.
 *
 * @tparam Generator the pseudo-random number generator type, it must meet the
 *   `UniformRandomBitGenerator` requirements.
 * @param gen the pseudo-random number generator.
 *
 * @par Example
 * @snippet storage_object_csek_samples.cc generate encryption key
 *
 * @see https://en.cppreference.com/w/cpp/numeric/random for a general
 *     overview of C++ pseudo-random number support.
 *
 * @see
 * https://en.cppreference.com/w/cpp/numeric/random/UniformRandomBitGenerator
 *     for a more detailed overview of the requirements for generators.
 */
template <typename Generator>
EncryptionKeyData CreateKeyFromGenerator(Generator& gen) {
  constexpr int kKeySize = 256 / std::numeric_limits<unsigned char>::digits;

  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr auto minchar = (std::numeric_limits<char>::min)();
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr auto maxchar = (std::numeric_limits<char>::max)();
  std::uniform_int_distribution<int> uni(minchar, maxchar);
  std::string key(static_cast<std::size_t>(kKeySize), ' ');
  std::generate_n(key.begin(), key.size(),
                  [&uni, &gen] { return static_cast<char>(uni(gen)); });
  return EncryptionDataFromBinaryKey(key);
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_WELL_KNOWN_HEADERS_H
