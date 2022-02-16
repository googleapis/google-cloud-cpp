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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OPENSSL_UTIL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OPENSSL_UTIL_H

#include "google/cloud/internal/base64_transforms.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <string>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Signs a string with the private key from a PEM container.
 *
 * @return Returns the signature as an *unencoded* byte array. The caller
 *   might want to use `Base64Encode()` or `HexEncode()` to convert this byte
 *   array to a format more suitable for transmission over HTTP.
 */
StatusOr<std::vector<std::uint8_t>> SignUsingSha256(
    std::string const& str, std::string const& pem_contents);

/**
 * Returns a Base64-encoded version of @p bytes. Using the URL- and
 * filesystem-safe alphabet, making these adjustments:
 * -  Replace '+' with '-'
 * -  Replace '/' with '_'
 * -  Right-trim '=' characters
 */
template <typename Collection>
inline std::string UrlsafeBase64Encode(Collection const& bytes) {
  Base64Encoder encoder;
  for (auto c : bytes) encoder.PushBack(c);
  std::string b64str = std::move(encoder).FlushAndPad();
  std::replace(b64str.begin(), b64str.end(), '+', '-');
  std::replace(b64str.begin(), b64str.end(), '/', '_');
  auto end_pos = b64str.find_last_not_of('=');
  if (std::string::npos != end_pos) {
    b64str.resize(end_pos + 1);
  }
  return b64str;
}

/**
 * Decodes a Url-safe Base64-encoded string.
 */
StatusOr<std::vector<std::uint8_t>> UrlsafeBase64Decode(std::string const& str);

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OPENSSL_UTIL_H
