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
#include <array>
#include <memory>
#include <type_traits>

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
  char const* sep = "";
  while (auto code = ERR_get_error()) {
    // OpenSSL guarantees that 256 bytes is enough:
    //   https://www.openssl.org/docs/man1.1.1/man3/ERR_error_string_n.html
    //   https://www.openssl.org/docs/man1.0.2/man3/ERR_error_string_n.html
    // we could not find a macro or constant to replace the 256 literal.
    auto constexpr kMaxOpenSslErrorLength = 256;
    std::array<char, kMaxOpenSslErrorLength> buf{};
    ERR_error_string_n(code, buf.data(), buf.size());
    msg += sep;
    msg += buf.data();
    sep = ", ";
  }
  return msg;
}

}  // namespace

StatusOr<std::vector<std::uint8_t>> SignUsingSha256(
    std::string const& str, std::string const& pem_contents) {
  ERR_clear_error();
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

  auto digest_ctx = GetDigestCtx();
  if (!digest_ctx) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials - "
                  "could not create context for OpenSSL digest: " +
                      CaptureSslErrors());
  }

  auto constexpr kOpenSslSuccess = 1;
  if (EVP_DigestSignInit(digest_ctx.get(), nullptr, EVP_sha256(), nullptr,
                         private_key.get()) != kOpenSslSuccess) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials - "
                  "could not initialize signing digest: " +
                      CaptureSslErrors());
  }

  if (EVP_DigestSignUpdate(digest_ctx.get(), str.data(), str.size()) !=
      kOpenSslSuccess) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials - could not sign blob: " +
                      CaptureSslErrors());
  }

  // The signed SHA256 size depends on the size (the experts say "modulus") of
  // they key.  First query the size:
  std::size_t actual_len = 0;
  if (EVP_DigestSignFinal(digest_ctx.get(), nullptr, &actual_len) !=
      kOpenSslSuccess) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials - could not sign blob: " +
                      CaptureSslErrors());
  }

  // Then compute the actual signed digest. Note that OpenSSL requires a
  // `unsigned char*` buffer, so we feed it that.
  std::vector<unsigned char> buffer(actual_len);
  if (EVP_DigestSignFinal(digest_ctx.get(), buffer.data(), &actual_len) !=
      kOpenSslSuccess) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials - could not sign blob: " +
                      CaptureSslErrors());
  }

  return StatusOr<std::vector<std::uint8_t>>(
      {buffer.begin(), std::next(buffer.begin(), actual_len)});
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
