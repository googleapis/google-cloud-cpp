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

#include "google/cloud/storage/internal/md5hash.h"
#include <algorithm>
#include <array>
#include <vector>
#ifdef _WIN32
#include <Windows.h>
#include <wincrypt.h>
#else
#include <openssl/evp.h>
#endif  // _WIN32

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::vector<std::uint8_t> MD5Hash(absl::string_view payload) {
#ifdef _WIN32
  std::vector<unsigned char> digest(16);
  BCryptHash(BCRYPT_MD5_ALG_HANDLE, nullptr, 0,
             reinterpret_cast<PUCHAR>(const_cast<char*>(payload.data())),
             static_cast<ULONG>(payload.size()), digest.data(),
             static_cast<ULONG>(digest.size()));
  return digest;
#else
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest;

  unsigned int size = 0;
  EVP_Digest(payload.data(), payload.size(), digest.data(), &size, EVP_md5(),
             nullptr);
  return std::vector<std::uint8_t>{digest.begin(),
                                   std::next(digest.begin(), size)};
#endif  // _WIN32
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
