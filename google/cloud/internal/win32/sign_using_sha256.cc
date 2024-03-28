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
#include "google/cloud/internal/sign_using_sha256.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/sha256_hash.h"
#include "google/cloud/internal/win32/win32_helpers.h"
#include "absl/strings/string_view.h"
#include <array>
#include <memory>
#include <type_traits>
#include <vector>
#include <Ntstatus.h>
#include <Windows.h>
#include <wincrypt.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

namespace {
StatusOr<std::vector<BYTE>> DecodePem(std::string const& pem_contents) {
  DWORD buffer_size = 0;
  if (!CryptStringToBinaryA(
          pem_contents.c_str(), static_cast<DWORD>(pem_contents.size()),
          CRYPT_STRING_BASE64HEADER, nullptr, &buffer_size, nullptr, nullptr)) {
    return InvalidArgumentError(
        FormatWin32Errors(
            "Invalid ServiceAccountCredentials - could not parse PEM to "
            "get private key: "),
        GCP_ERROR_INFO());
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
    return InvalidArgumentError(
        FormatWin32Errors(
            "Invalid ServiceAccountCredentials - could not parse PKCS#8 to "
            "get private key: "),
        GCP_ERROR_INFO());
  }
  std::unique_ptr<CRYPT_PRIVATE_KEY_INFO, decltype(&LocalFree)>
      private_key_info(private_key_info_raw, &LocalFree);
  if (absl::string_view(private_key_info->Algorithm.pszObjId) !=
      szOID_RSA_RSA) {
    return InvalidArgumentError(
        absl::StrCat("Invalid ServiceAccountCredentials - not an RSA key, "
                     "algorithm is: ",
                     absl::string_view(private_key_info->Algorithm.pszObjId)),
        GCP_ERROR_INFO());
  }
  DWORD rsa_blob_size;
  if (!CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                           CNG_RSA_PRIVATE_KEY_BLOB,
                           private_key_info->PrivateKey.pbData,
                           private_key_info->PrivateKey.cbData, 0, nullptr,
                           nullptr, &rsa_blob_size)) {
    return InvalidArgumentError(
        FormatWin32Errors(
            "Invalid ServiceAccountCredentials - could not decode RSA key: "),
        GCP_ERROR_INFO());
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
    return InvalidArgumentError(
        FormatWin32Errors(
            "Invalid ServiceAccountCredentials - could not import RSA key: "),
        GCP_ERROR_INFO());
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
    return InvalidArgumentError(
        FormatWin32Errors(
            "Invalid ServiceAccountCredentials - could not sign blob: "),
        GCP_ERROR_INFO());
  }
  std::vector<std::uint8_t> signature(signature_size);
  BCryptSignHash(key, &padding_info, const_cast<PUCHAR>(digest.data()),
                 static_cast<DWORD>(digest.size()), signature.data(),
                 signature_size, &signature_size, BCRYPT_PAD_PKCS1);
  return signature;
}

}  // namespace

StatusOr<std::vector<std::uint8_t>> SignUsingSha256(
    std::string const& str, std::string const& pem_contents) {
  auto pem_buffer = DecodePem(pem_contents);
  if (!pem_buffer) return std::move(pem_buffer).status();

  auto rsa_blob = GetCngPrivateKeyBlobFromPkcsBuffer(std::move(*pem_buffer));
  if (!rsa_blob) return std::move(rsa_blob).status();

  auto key = CreateRsaBCryptKey(std::move(*rsa_blob));
  if (!key) return std::move(key).status();

  auto hash = Sha256Hash(str);

  return SignSha256Digest(key->get(), hash);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // _WIN32
