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
#include <type_traits>
#include <Windows.h>
#include <wincrypt.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {
struct ContextDeleter {
  void operator()(BCRYPT_HASH_HANDLE h) const { BCryptDestroyHash(h); }
};

using ContextPtr =
    std::unique_ptr<std::remove_pointer_t<BCRYPT_HASH_HANDLE>, ContextDeleter>;

ContextPtr CreateMD5HashCtx() {
  BCRYPT_HASH_HANDLE hHash = nullptr;
  BCryptCreateHash(BCRYPT_MD5_ALG_HANDLE, &hHash, nullptr, 0, nullptr, 0, 0);
  return ContextPtr(hHash);
}

class BCryptMD5HashFunction : public MD5HashFunction {
 public:
  BCryptMD5HashFunction() : impl_(CreateMD5HashCtx()) {}

  void Update(absl::string_view buffer) override {
    BCryptHashData(impl_.get(),
                   reinterpret_cast<PUCHAR>(const_cast<char*>(buffer.data())),
                   static_cast<ULONG>(buffer.size()), 0);
  }

  Hash FinishImpl() override {
    MD5HashFunction::Hash hash;
    BCryptFinishHash(impl_.get(), reinterpret_cast<PUCHAR>(hash.data()),
                     static_cast<ULONG>(hash.size()), 0);
    return hash;
  }

 private:
  ContextPtr impl_;
};
}  // namespace

std::unique_ptr<MD5HashFunction> MD5HashFunction::Create() {
  return std::make_unique<BCryptMD5HashFunction>();
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
#endif  // _WIN32
