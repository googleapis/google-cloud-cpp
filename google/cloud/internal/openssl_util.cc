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
#ifdef _WIN32
#include "google/cloud/internal/sha256_hash.h"
#include "absl/strings/string_view.h"
#include <type_traits>
#include <Ntstatus.h>
#include <Windows.h>
#include <wincrypt.h>
#else
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/opensslv.h>
#include <openssl/pem.h>
#endif
#include <array>
#include <memory>
#include <type_traits>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

namespace {
#ifdef _WIN32
std::string CaptureWin32Errors() {
  auto last_error = GetLastError();
  LPSTR message_buffer_raw = nullptr;
  auto size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, last_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&message_buffer_raw, 0, nullptr);
  std::unique_ptr<char, decltype(&LocalFree)> message_buffer(message_buffer_raw,
                                                             &LocalFree);
  return std::string(message_buffer.get(), size) + " (error code " +
         std::to_string(last_error) + ")";
}

StatusOr<std::vector<BYTE>> DecodePem(std::string const& pem_contents) {
  DWORD buffer_size = 0;
  if (!CryptStringToBinaryA(
          pem_contents.c_str(), static_cast<DWORD>(pem_contents.size()),
          CRYPT_STRING_BASE64HEADER, nullptr, &buffer_size, nullptr, nullptr)) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials - could not parse PEM to "
                  "get private key: " +
                      CaptureWin32Errors());
  }
  std::vector<BYTE> buffer(buffer_size);
  CryptStringToBinaryA(
      pem_contents.c_str(), static_cast<DWORD>(pem_contents.size()),
      CRYPT_STRING_BASE64HEADER, buffer.data(), &buffer_size, nullptr, nullptr);
  return buffer;
}

StatusOr<std::vector<BYTE>> GetCngPrivateKeyBlobFromPkcsBuffer(
    std::vector<BYTE> pkcs8_buffer) {
  PCRYPT_PRIVATE_KEY_INFO private_key_info_raw;
  DWORD private_key_info_size;
  if (!CryptDecodeObjectEx(
          X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, PKCS_PRIVATE_KEY_INFO,
          pkcs8_buffer.data(), static_cast<DWORD>(pkcs8_buffer.size()),
          CRYPT_DECODE_NOCOPY_FLAG | CRYPT_DECODE_ALLOC_FLAG, nullptr,
          &private_key_info_raw, &private_key_info_size)) {
    return Status(
        StatusCode::kInvalidArgument,
        "Invalid ServiceAccountCredentials - could not parse PKCS#8 to "
        "get private key: " +
            CaptureWin32Errors());
  }
  std::unique_ptr<CRYPT_PRIVATE_KEY_INFO, decltype(&LocalFree)>
      private_key_info(private_key_info_raw, &LocalFree);
  if (absl::string_view(private_key_info->Algorithm.pszObjId) !=
      szOID_RSA_RSA) {
    return Status(
        StatusCode::kInvalidArgument,
        "Invalid ServiceAccountCredentials - not an RSA key, algorithm is: " +
            std::string(private_key_info->Algorithm.pszObjId));
  }
  DWORD rsa_blob_size;
  if (!CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                           CNG_RSA_PRIVATE_KEY_BLOB,
                           private_key_info->PrivateKey.pbData,
                           private_key_info->PrivateKey.cbData, 0, nullptr,
                           nullptr, &rsa_blob_size)) {
    return Status(
        StatusCode::kInvalidArgument,
        "Invalid ServiceAccountCredentials - could not decode RSA key: " +
            CaptureWin32Errors());
  }
  std::vector<BYTE> rsa_blob(rsa_blob_size);
  CryptDecodeObjectEx(
      X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, CNG_RSA_PRIVATE_KEY_BLOB,
      private_key_info->PrivateKey.pbData, private_key_info->PrivateKey.cbData,
      0, nullptr, rsa_blob.data(), &rsa_blob_size);
  return rsa_blob;
}

StatusOr<std::unique_ptr<std::remove_pointer_t<BCRYPT_KEY_HANDLE>,
                         decltype(&BCryptDestroyKey)>>
CreateRsaBCryptKey(std::vector<BYTE> buffer) {
  BCRYPT_KEY_HANDLE key_handle;
  if (BCryptImportKeyPair(BCRYPT_RSA_ALG_HANDLE, nullptr,
                          BCRYPT_RSAPRIVATE_BLOB, &key_handle, buffer.data(),
                          static_cast<DWORD>(buffer.size()),
                          0) != STATUS_SUCCESS) {
    return Status(
        StatusCode::kInvalidArgument,
        "Invalid ServiceAccountCredentials - could not import RSA key: " +
            CaptureWin32Errors());
  }
  return std::unique_ptr<std::remove_pointer_t<BCRYPT_KEY_HANDLE>,
                         decltype(&BCryptDestroyKey)>(key_handle,
                                                      &BCryptDestroyKey);
}

StatusOr<std::vector<std::uint8_t>> SignSha256Digest(BCRYPT_KEY_HANDLE key,
                                                     Sha256Type const& digest) {
  DWORD signature_size;
  BCRYPT_PKCS1_PADDING_INFO padding_info;
  padding_info.pszAlgId = BCRYPT_SHA256_ALGORITHM;
  if (BCryptSignHash(key, &padding_info, const_cast<PUCHAR>(digest.data()),
                     static_cast<DWORD>(digest.size()), nullptr, 0,
                     &signature_size, BCRYPT_PAD_PKCS1) != STATUS_SUCCESS) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials - could not sign blob: " +
                      CaptureWin32Errors());
  }
  std::vector<std::uint8_t> signature(signature_size);
  BCryptSignHash(key, &padding_info, const_cast<PUCHAR>(digest.data()),
                 static_cast<DWORD>(digest.size()), signature.data(),
                 signature_size, &signature_size, BCRYPT_PAD_PKCS1);
  return signature;
}
#else
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
#endif  // _WIN32

}  // namespace

#ifdef _WIN32
StatusOr<std::vector<std::uint8_t>> SignUsingSha256(
    std::string const& str, std::string const& pem_contents) {
  auto pem_buffer = DecodePem(pem_contents);
  if (!pem_buffer) return pem_buffer.status();

  auto rsa_blob = GetCngPrivateKeyBlobFromPkcsBuffer(std::move(*pem_buffer));
  if (!rsa_blob) return rsa_blob.status();

  auto key = CreateRsaBCryptKey(std::move(*rsa_blob));
  if (!key) return key.status();

  auto hash = Sha256Hash(str);

  return SignSha256Digest(key->get(), hash);
}
#else
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
#endif  // _WIN32

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
