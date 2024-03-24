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

#ifndef _WIN32
#include "google/cloud/storage/internal/hash_function_impl.h"
#include <openssl/evp.h>
#include <type_traits>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {
struct ContextDeleter {
  void operator()(EVP_MD_CTX* context) const {
// The name of the function to free an EVP_MD_CTX changed in OpenSSL 1.1.0.
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)  // Older than version 1.1.0.
    EVP_MD_CTX_destroy(context);
#else
    EVP_MD_CTX_free(context);
#endif  // OPENSSL_VERSION_NUMBER >= 0x10100000L
  }
};

using ContextPtr =
    std::unique_ptr<std::remove_pointer_t<EVP_MD_CTX>, ContextDeleter>;

ContextPtr CreateDigestCtx() {
// The name of the function to create and delete EVP_MD_CTX objects changed
// with OpenSSL 1.1.0.
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)  // Older than version 1.1.0.
  return ContextPtr(EVP_MD_CTX_create());
#else
  return ContextPtr(EVP_MD_CTX_new());
#endif
}

class OpenSslMD5HashFunction : public MD5HashFunction {
 public:
  OpenSslMD5HashFunction() : impl_(CreateDigestCtx()) {
    EVP_DigestInit_ex(impl_.get(), EVP_md5(), nullptr);
  }

  void Update(absl::string_view buffer) override {
    EVP_DigestUpdate(impl_.get(), buffer.data(), buffer.size());
  }

  Hash FinishImpl() override {
    Hash hash;
    unsigned int len = 0;
    // Note: EVP_DigestFinal_ex() consumes an `unsigned char*` for the output
    // array.  On some platforms (read PowerPC and ARM), the default `char` is
    // unsigned. In those platforms it is possible that
    // `std::uint8_t != unsigned char` and the `reinterpret_cast<>` is needed.
    // It should be safe in any case.
    EVP_DigestFinal_ex(impl_.get(),
                       reinterpret_cast<unsigned char*>(hash.data()), &len);
    return hash;
  }

 private:
  ContextPtr impl_;
};
}  // namespace

std::unique_ptr<MD5HashFunction> MD5HashFunction::Create() {
  return std::make_unique<OpenSslMD5HashFunction>();
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
#endif  // _WIN32
