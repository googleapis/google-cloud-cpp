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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OPENSSL_UTIL_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OPENSSL_UTIL_H_

#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/storage/oauth2/credential_constants.h"
#include "google/cloud/storage/version.h"
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>
#include <openssl/pem.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

/**
 * Decodes a Base64-encoded string.
 */
std::string Base64Decode(std::string const& str);

/**
 * Encodes a string using Base64.
 */
std::string Base64Encode(std::string const& str);

/**
 * Encodes a string using Base64.
 */
std::string Base64Encode(std::vector<std::uint8_t> const& bytes);

/**
 * Signs a string with the private key from a PEM container.
 *
 * @return Returns the signature as an *unencoded* byte array. The caller
 *   might want to use `Base64Encode()` or `HexEncode()` to convert this byte
 *   array to a format more suitable for transmission over HTTP.
 */
std::vector<std::uint8_t> SignStringWithPem(
    std::string const& str, std::string const& pem_contents,
    storage::oauth2::JwtSigningAlgorithms alg);

/**
 * Transforms a string in-place, removing trailing occurrences of a character.
 *
 * Warning: this was written with the intent of operating on a string
 * containing ASCII-encoded (8-bit) characters (e.g. removing trailing '='
 * characters from a Base64-encoded string) and may not function correctly
 * for strings containing Unicode characters.
 */
inline void RightTrim(std::string& str, char trim_ch) {
  auto end_pos = str.find_last_not_of(trim_ch);
  if (std::string::npos != end_pos) str.resize(end_pos + 1);
}

/**
 * Returns a Base64-encoded version of the given a string, using the URL- and
 * filesystem-safe alphabet, making these adjustments:
 * -  Replace '+' with '-'
 * -  Replace '/' with '_'
 * -  Right-trim '=' characters
 */
inline std::string UrlsafeBase64Encode(std::vector<std::uint8_t> const& bytes) {
  std::string b64str = Base64Encode(bytes);
  std::replace(b64str.begin(), b64str.end(), '+', '-');
  std::replace(b64str.begin(), b64str.end(), '/', '_');
  RightTrim(b64str, '=');
  return b64str;
}

inline std::string UrlsafeBase64Encode(std::string const& str) {
  std::string b64str = Base64Encode(str);
  std::replace(b64str.begin(), b64str.end(), '+', '-');
  std::replace(b64str.begin(), b64str.end(), '/', '_');
  RightTrim(b64str, '=');
  return b64str;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OPENSSL_UTIL_H_
