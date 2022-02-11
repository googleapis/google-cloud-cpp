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
// The name of the function to free an EVP_MD_CTX changed in OpenSSL 1.1.0.
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)  // Older than version 1.1.0.
inline std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_destroy)>
GetDigestCtx() {
  return std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_destroy)>(
      EVP_MD_CTX_create(), &EVP_MD_CTX_destroy);
};
#else
inline std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> GetDigestCtx() {
  return std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>(
      EVP_MD_CTX_new(), &EVP_MD_CTX_free);
};
#endif
}  // namespace

StatusOr<std::vector<std::uint8_t>> Base64Decode(std::string const& str) {
  return google::cloud::internal::Base64DecodeToBytes(str);
}

std::string Base64Encode(std::string const& str) {
  google::cloud::internal::Base64Encoder enc;
  for (auto c : str) enc.PushBack(c);
  return std::move(enc).FlushAndPad();
}

std::string Base64Encode(std::vector<std::uint8_t> const& bytes) {
  google::cloud::internal::Base64Encoder enc;
  for (auto c : bytes) enc.PushBack(c);
  return std::move(enc).FlushAndPad();
}

StatusOr<std::vector<std::uint8_t>> SignStringWithPem(
    std::string const& str, std::string const& pem_contents,
    oauth2_internal::JwtSigningAlgorithms alg) {
  using ::google::cloud::oauth2_internal::JwtSigningAlgorithms;

  auto digest_ctx = GetDigestCtx();
  if (!digest_ctx) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials: "
                  "could not create context for OpenSSL digest. ");
  }

  EVP_MD const* digest_type = nullptr;
  switch (alg) {
    case JwtSigningAlgorithms::RS256:
      digest_type = EVP_sha256();
      break;
  }
  if (digest_type == nullptr) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials: "
                  "could not find specified digest in OpenSSL. ");
  }

  auto pem_buffer = std::unique_ptr<BIO, decltype(&BIO_free)>(
      BIO_new_mem_buf(pem_contents.data(),
                      static_cast<int>(pem_contents.length())),
      &BIO_free);
  if (!pem_buffer) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials: "
                  "could not create PEM buffer. ");
  }

  auto private_key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(
      PEM_read_bio_PrivateKey(
          pem_buffer.get(),
          nullptr,  // EVP_PKEY **x
          nullptr,  // pem_password_cb *cb -- a custom callback.
          // void *u -- this represents the password for the PEM (only
          // applicable for formats such as PKCS12 (.p12 files) that use
          // a password, which we don't currently support.
          nullptr),
      &EVP_PKEY_free);
  if (!private_key) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials: "
                  "could not parse PEM to get private key ");
  }

  int const digest_sign_success_code = 1;
  if (digest_sign_success_code !=
      EVP_DigestSignInit(digest_ctx.get(),
                         nullptr,  // `EVP_PKEY_CTX **pctx`
                         digest_type,
                         nullptr,  // `ENGINE *e`
                         private_key.get())) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials: "
                  "could not initialize PEM digest. ");
  }

  if (digest_sign_success_code !=
      EVP_DigestSignUpdate(digest_ctx.get(), str.data(), str.length())) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials: "
                  "could not update PEM digest. ");
  }

  std::size_t signed_str_size = 0;
  // Calling this method with a nullptr buffer will populate our size var
  // with the resulting buffer's size. This allows us to then call it again,
  // with the correct buffer and size, which actually populates the buffer.
  if (digest_sign_success_code !=
      EVP_DigestSignFinal(digest_ctx.get(),
                          nullptr,  // unsigned char *sig
                          &signed_str_size)) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials: "
                  "could not finalize PEM digest (1/2). ");
  }

  std::vector<unsigned char> signed_str(signed_str_size);
  if (digest_sign_success_code != EVP_DigestSignFinal(digest_ctx.get(),
                                                      signed_str.data(),
                                                      &signed_str_size)) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials: "
                  "could not finalize PEM digest (2/2). ");
  }

  return StatusOr<std::vector<unsigned char>>(
      {signed_str.begin(), signed_str.end()});
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

std::vector<std::uint8_t> MD5Hash(std::string const& payload) {
  MD5_CTX md5;
  MD5_Init(&md5);
  MD5_Update(&md5, payload.c_str(), payload.size());

  std::vector<std::uint8_t> hash(MD5_DIGEST_LENGTH, 0);
  // Note: MD5_Final consumes a `unsigned char*` in its first parameter, on some
  // platforms (PowerPC and ARM I read), the default `char` is unsigned. In
  // those platforms it is possible that `std::uint8_t != unsigned char` and
  // the `reinterpret_cast<>` is not trivial (but still safe I think).
  MD5_Final(reinterpret_cast<unsigned char*>(hash.data()), &md5);
  return hash;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
