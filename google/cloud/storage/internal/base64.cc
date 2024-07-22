// Copyright 2019 Google LLC
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

#include "google/cloud/storage/internal/base64.h"
#include "google/cloud/internal/base64_transforms.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

StatusOr<std::vector<std::uint8_t>> Base64Decode(std::string const& str) {
  return google::cloud::internal::Base64DecodeToBytes(str);
}

std::string Base64Encode(std::string const& str) {
  google::cloud::internal::Base64Encoder enc;
  for (auto c : str) enc.PushBack(c);
  return std::move(enc).FlushAndPad();
}

std::string Base64Encode(absl::Span<std::uint8_t const> bytes) {
  google::cloud::internal::Base64Encoder enc;
  for (auto c : bytes) enc.PushBack(c);
  return std::move(enc).FlushAndPad();
}

StatusOr<std::vector<std::uint8_t>> UrlsafeBase64Decode(
    std::string const& str) {
  if (str.empty()) return std::vector<std::uint8_t>{};
  std::string b64str = str;
  std::replace(b64str.begin(), b64str.end(), '-', '+');
  std::replace(b64str.begin(), b64str.end(), '_', '/');
  // To restore the padding there are only two cases:
  //    https://en.wikipedia.org/wiki/Base64#Decoding_Base64_without_padding
  if (b64str.length() % 4 == 2) {
    b64str.append("==");
  } else if (b64str.length() % 4 == 3) {
    b64str.append("=");
  }
  return Base64Decode(b64str);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
