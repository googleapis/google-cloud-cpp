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

#include "google/cloud/internal/openssl_util.h"
#include "google/cloud/internal/base64_transforms.h"
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/opensslv.h>
#include <openssl/pem.h>
#include <memory>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

namespace {
struct OpenSslDeleter {
  void operator()(EVP_MD_CTX* ptr) {
// The name of the function to free an EVP_MD_CTX changed in OpenSSL 1.1.0.
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)  // Older than version 1.1.0.
    EVP_MD_CTX_destroy(ptr);
#else
    EVP_MD_CTX_free(ptr);
#endif
  }

  void operator()(EVP_PKEY* ptr) { EVP_PKEY_free(ptr); }
  void operator()(BIO* ptr) { BIO_free(ptr); }
};

std::unique_ptr<EVP_MD_CTX, OpenSslDeleter> GetDigestCtx() {
// The name of the function to create an EVP_MD_CTX changed in OpenSSL 1.1.0.
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)  // Older than version 1.1.0.
  return std::unique_ptr<EVP_MD_CTX, OpenSslDeleter>(EVP_MD_CTX_create());
#else
  return std::unique_ptr<EVP_MD_CTX, OpenSslDeleter>(EVP_MD_CTX_new());
#endif
}

std::string CaptureSslErrors() {
  std::string msg;
  while (auto code = ERR_get_error()) {
    // OpenSSL guarantees that 256 bytes is enough:
    //   https://www.openssl.org/docs/man1.1.1/man3/ERR_error_string_n.html
    //   https://www.openssl.org/docs/man1.0.2/man3/ERR_error_string_n.html
    // we could not find a macro or constant to replace the 256 literal.
    auto constexpr kMaxOpenSslErrorLength = 256;
    std::array<char, kMaxOpenSslErrorLength> buf{};
    ERR_error_string_n(code, buf.data(), buf.size());
    msg += buf.data();
  }
  return msg;
}

}  // namespace

StatusOr<std::vector<std::uint8_t>> SignUsingSha256(
    std::string const& str, std::string const& pem_contents) {
  auto pem_buffer = std::unique_ptr<BIO, OpenSslDeleter>(BIO_new_mem_buf(
      pem_contents.data(), static_cast<int>(pem_contents.length())));
  if (!pem_buffer) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials - "
                  "could not create PEM buffer: " +
                      CaptureSslErrors());
  }

  auto private_key =
      std::unique_ptr<EVP_PKEY, OpenSslDeleter>(PEM_read_bio_PrivateKey(
          pem_buffer.get(),
          nullptr,  // EVP_PKEY **x
          nullptr,  // pem_password_cb *cb -- a custom callback.
          // void *u -- this represents the password for the PEM (only
          // applicable for formats such as PKCS12 (.p12 files) that use
          // a password, which we don't currently support.
          nullptr));
  if (!private_key) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials - "
                  "could not parse PEM to get private key: " +
                      CaptureSslErrors());
  }

  auto const kOpenSslSuccess = 1;

  auto digest_ctx = GetDigestCtx();
  if (!digest_ctx) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials - "
                  "could not create context for OpenSSL digest: " +
                      CaptureSslErrors());
  }
  if (EVP_DigestSignInit(digest_ctx.get(), nullptr, EVP_sha256(), nullptr,
                         private_key.get()) != kOpenSslSuccess) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials - "
                  "could not initialize signing digest: " +
                      CaptureSslErrors());
  }

  // The signed SHA256 size depends on the size (the experts say "modulus") of
  // they key.  First query the size:
   /*C API, no std::*/ size_t actual_len = 0;
  if (EVP_DigestSign(digest_ctx.get(), nullptr, &actual_len,
                     reinterpret_cast<unsigned char const*>(str.data()),
                     str.size()) != kOpenSslSuccess) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials - "
                  "could not sign blob: " +
                      CaptureSslErrors());
  }

  // Then compute the actual signed digest:
  std::vector<unsigned char> buffer(actual_len);
  if (EVP_DigestSign(digest_ctx.get(), nullptr, &actual_len,
                     reinterpret_cast<unsigned char const*>(str.data()),
                     str.size()) != kOpenSslSuccess) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials - "
                  "could not sign blob: " +
                      CaptureSslErrors());
  }

  return StatusOr<std::vector<std::uint8_t>>(
      {buffer.begin(), std::next(buffer.begin(), actual_len)});
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
  return Base64DecodeToBytes(b64str);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
