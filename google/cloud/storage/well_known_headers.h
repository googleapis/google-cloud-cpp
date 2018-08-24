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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_WELL_KNOWN_HEADERS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_WELL_KNOWN_HEADERS_H_

#include "google/cloud/internal/optional.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/storage/version.h"
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Refactor definition of well-known request headers using the CRTP.
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

 private:
  google::cloud::internal::optional<T> value_;
};

template <typename H, typename T>
std::ostream& operator<<(std::ostream& os, WellKnownHeader<H, T> const& rhs) {
  if (rhs.has_value()) {
    return os << rhs.header_name() << ": " << rhs.value();
  }
  return os << rhs.header_name() << ": <not set>";
}

struct ContentType : public WellKnownHeader<ContentType, std::string> {
  using WellKnownHeader<ContentType, std::string>::WellKnownHeader;
  static char const* header_name() { return "content-type"; }
};

struct IfMatchEtag : public WellKnownHeader<IfMatchEtag, std::string> {
  using WellKnownHeader<IfMatchEtag, std::string>::WellKnownHeader;
  static char const* header_name() { return "If-Match"; }
};

struct IfNoneMatchEtag : public WellKnownHeader<IfNoneMatchEtag, std::string> {
  using WellKnownHeader<IfNoneMatchEtag, std::string>::WellKnownHeader;
  static char const* header_name() { return "If-None-Match"; }
};

struct EncryptionKeyData {
  std::string algorithm;
  std::string key;
  std::string sha256;
};

/**
 * Format a (potentially binary) encryption key in the format required by the
 * Google Cloud Storage API.
 *
 * @param key a binary key, must have exactly 32 bytes.
 */
EncryptionKeyData EncryptionDataFromBinaryKey(std::string const& key);

struct EncryptionKey
    : public WellKnownHeader<EncryptionKey, EncryptionKeyData> {
  using WellKnownHeader<EncryptionKey, EncryptionKeyData>::WellKnownHeader;

  /**
   * Create an encryption key parameter from a binary key.
   *
   * @param key a binary key, must have exactly 32 bytes.
   */
  static EncryptionKey FromBinaryKey(std::string const& key);

  static char const* prefix() { return "x-goog-encryption-"; }
};

std::ostream& operator<<(std::ostream& os, EncryptionKey const& rhs);

struct SourceEncryptionKey
    : public WellKnownHeader<SourceEncryptionKey, EncryptionKeyData> {
  using WellKnownHeader<SourceEncryptionKey,
                        EncryptionKeyData>::WellKnownHeader;

  /**
   * Create a source encryption key parameter from a binary key.
   *
   * @param key a binary key, must have exactly 32 bytes.
   */
  static SourceEncryptionKey FromBinaryKey(std::string const& key);

  static char const* prefix() { return "x-goog-copy-source-encryption-"; }
};

std::ostream& operator<<(std::ostream& os, SourceEncryptionKey const& rhs);

/**
 * Create an encryption key parameter from a pseudo-random number generator.
 *
 * @tparam Generator the pseudo-random number generator type, it must meet the
 *   `UniformRandomBitGenerator` requirements.
 * @param gen the pseudo-random number generator.
 *
 * @snippet storage_object_samples.cc generate encryption key
 *
 * @see https://en.cppreference.com/w/cpp/numeric/random for a general
 *     overview of C++ pseudo-random number support.
 *
 * @see
 * https://en.cppreference.com/w/cpp/numeric/random/UniformRandomBitGenerator
 *     for a more detailed overview of the requirements for generators.
 */
template <typename Generator>
static EncryptionKeyData CreateKeyFromGenerator(Generator& gen) {
  static_assert(
      std::numeric_limits<unsigned char>::digits == 8,
      "The Google Cloud Storage C++ library is only supported on platforms\n"
      "with 8-bit chars.  Please file a bug on\n"
      "    https://github.com/GoogleCloudPlatform/google-cloud-cpp/issues\n"
      "describing your platform details to request support for it.");
  constexpr int kKeySize = 256 / std::numeric_limits<unsigned char>::digits;

  constexpr auto minchar = std::numeric_limits<char>::min();
  constexpr auto maxchar = std::numeric_limits<char>::max();
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

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_WELL_KNOWN_HEADERS_H_
