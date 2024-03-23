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

#ifdef _WIN32
#include "google/cloud/storage/internal/hash_function_impl.h"
#include "google/cloud/storage/internal/base64.h"
#include <Windows.h>
#include <wincrypt.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {
MD5HashFunction::ContextPtr CreateMD5HashCtx() {
  BCRYPT_HASH_HANDLE hHash = nullptr;
  BCryptCreateHash(BCRYPT_MD5_ALG_HANDLE, &hHash, nullptr, 0, nullptr, 0, 0);
  return MD5HashFunction::ContextPtr(hHash);
}
}  // namespace

static_assert(std::is_same_v<MD5Context, BCRYPT_HASH_HANDLE>,
              "MD5Context is not the same type as BCRYPT_HASH_HANDLE");

MD5HashFunction::MD5HashFunction() : impl_(CreateMD5HashCtx()) {}

void MD5HashFunction::Update(absl::string_view buffer) {
  BCryptHashData(impl_.get(),
                 reinterpret_cast<PUCHAR>(const_cast<char*>(buffer.data())),
                 static_cast<ULONG>(buffer.size()), 0);
}

HashValues MD5HashFunction::Finish() {
  if (hashes_.has_value()) return *hashes_;
  // (8 bits per byte) * 16 bytes = 128 bits
  std::array<std::uint8_t, 16> hash;
  BCryptFinishHash(impl_.get(), reinterpret_cast<PUCHAR>(hash.data()),
                   static_cast<ULONG>(hash.size()), 0);
  hashes_ = HashValues{/*.crc32c=*/{}, /*.md5=*/Base64Encode(hash)};
  return *hashes_;
}

void MD5HashFunction::ContextDeleter::operator()(MD5Context context) {
  BCryptDestroyHash(context);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
#endif  // _WIN32
