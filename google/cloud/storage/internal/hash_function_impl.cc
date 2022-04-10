// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/hash_function_impl.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/internal/big_endian.h"
#include "absl/memory/memory.h"
#include <crc32c/crc32c.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ContextPtr = std::unique_ptr<EVP_MD_CTX, MD5HashFunction::ContextDeleter>;

ContextPtr CreateDigestCtx() {
// The name of the function to create and delete EVP_MD_CTX objects changed
// with OpenSSL 1.1.0.
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)  // Older than version 1.1.0.
  return ContextPtr(EVP_MD_CTX_create());
#else
  return ContextPtr(EVP_MD_CTX_new());
#endif
}

void DeleteDigestCtx(EVP_MD_CTX* context) {
// The name of the function to free an EVP_MD_CTX changed in OpenSSL 1.1.0.
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)  // Older than version 1.1.0.
  EVP_MD_CTX_destroy(context);
#else
  EVP_MD_CTX_free(context);
#endif  // OPENSSL_VERSION_NUMBER >= 0x10100000L
}

}  // namespace

std::string CompositeFunction::Name() const {
  return "composite(" + a_->Name() + "," + b_->Name() + ")";
}

void CompositeFunction::Update(char const* buf, std::size_t n) {
  a_->Update(buf, n);
  b_->Update(buf, n);
}

HashValues CompositeFunction::Finish() && {
  return Merge(std::move(*a_).Finish(), std::move(*b_).Finish());
}

MD5HashFunction::MD5HashFunction() : impl_(CreateDigestCtx()) {
  EVP_DigestInit_ex(impl_.get(), EVP_md5(), nullptr);
}

void MD5HashFunction::Update(char const* buf, std::size_t n) {
  EVP_DigestUpdate(impl_.get(), buf, n);
}

HashValues MD5HashFunction::Finish() && {
  std::vector<std::uint8_t> hash(EVP_MD_size(EVP_md5()));
  unsigned int len = 0;
  // Note: EVP_DigestFinal_ex() consumes a `unsigned char*` for the output array
  // (digest.data()) in our case.  On some platforms (PowerPC and ARM read), the
  // default `char` is unsigned. In those platforms it is possible that
  // `std::uint8_t != unsigned char` and the `reinterpret_cast<>` is needed, but
  // it is still safe.
  EVP_DigestFinal_ex(impl_.get(), reinterpret_cast<unsigned char*>(hash.data()),
                     &len);
  hash.resize(len);
  return HashValues{/*.crc32c=*/{}, /*.md5=*/Base64Encode(hash)};
}

void MD5HashFunction::ContextDeleter::operator()(EVP_MD_CTX* context) {
  DeleteDigestCtx(context);
}

void Crc32cHashFunction::Update(char const* buf, std::size_t n) {
  current_ =
      crc32c::Extend(current_, reinterpret_cast<std::uint8_t const*>(buf), n);
}

HashValues Crc32cHashFunction::Finish() && {
  std::string const hash = google::cloud::internal::EncodeBigEndian(current_);
  return HashValues{/*.crc32c=*/Base64Encode(hash), /*.md5=*/{}};
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
