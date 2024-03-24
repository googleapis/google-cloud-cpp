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
#include "google/cloud/internal/oauth2_service_account_credentials.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/win32/win32_helpers.h"
#include <fstream>
#include <type_traits>
#include <vector>
#include <Windows.h>
#include <wincrypt.h>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::internal::FormatWin32Errors;
using ::google::cloud::internal::InvalidArgumentError;

StatusOr<ServiceAccountCredentialsInfo> ParseServiceAccountP12File(
    std::string const& source) {
  // Read the PKCS#12 file into memory.
  std::vector<char> data;
  {
    std::ifstream file(source, std::ios::binary);
    if (!file.is_open()) {
      return InvalidArgumentError(
          absl::StrCat("Cannot open PKCS#12 file (", source, ")"),
          GCP_ERROR_INFO());
    }
    data.assign(std::istreambuf_iterator<char>{file}, {});
  }

  // Import the PKCS#12 file into a certificate store.
  HCERTSTORE certstore_raw = [data = std::move(data)]() mutable {
    CRYPT_DATA_BLOB dataBlob = {static_cast<DWORD>(data.size()),
                                reinterpret_cast<BYTE*>(data.data())};
    return PFXImportCertStore(&dataBlob, L"notasecret", CRYPT_EXPORTABLE);
  }();
  if (certstore_raw == nullptr) {
    return InvalidArgumentError(
        FormatWin32Errors("Cannot parse PKCS#12 file (", source, "): "),
        GCP_ERROR_INFO());
  }
  auto close_certstore = [](HCERTSTORE certstore) {
    CertCloseStore(certstore, 0);
  };
  std::unique_ptr<void, decltype(close_certstore)> certstore(certstore_raw,
                                                             close_certstore);

  // Get the certificate from the store.
  PCCERT_CONTEXT cert_raw = CertFindCertificateInStore(
      certstore.get(), X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0,
      CERT_FIND_ANY, nullptr, nullptr);
  if (cert_raw == nullptr) {
    return InvalidArgumentError(
        absl::StrCat("No certificate found in PKCS#12 file (", source, ")"),
        GCP_ERROR_INFO());
  }
  std::unique_ptr<std::remove_pointer_t<decltype(cert_raw)>,
                  decltype(&CertFreeCertificateContext)>
      cert(cert_raw, &CertFreeCertificateContext);

  // Get the service account ID from the certificate's common name.
  std::string service_account_id = [](PCCERT_CONTEXT cert) {
    DWORD size = CertGetNameStringA(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0,
                                    nullptr, nullptr, 0);
    std::string result(size, '\0');
    CertGetNameStringA(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr,
                       &result[0], size);
    result.pop_back();  // remove the null terminator
    return result;
  }(cert.get());

  // Validate the service account ID.
  if (service_account_id.find_first_not_of("0123456789") != std::string::npos ||
      service_account_id.empty()) {
    return InvalidArgumentError(
        absl::StrCat(
            "Invalid PKCS#12 file (", source,
            "): service account id missing or not not formatted correctly"),
        GCP_ERROR_INFO());
  }

  // Get a provider that has the private key of the certificate.
  HCRYPTPROV prov_raw;
  DWORD pdwKeySpec;
  BOOL pfCallerFreeProvOrNCryptKey;
  if (!CryptAcquireCertificatePrivateKey(cert.get(), CRYPT_ACQUIRE_SILENT_FLAG,
                                         nullptr, &prov_raw, &pdwKeySpec,
                                         &pfCallerFreeProvOrNCryptKey)) {
    return InvalidArgumentError(
        FormatWin32Errors("No private key found in PKCS#12 file (", source,
                          "): "),
        GCP_ERROR_INFO());
  }
  auto close_prov = [](void* prov) {
    // According to documentation of CryptAcquireCertificatePrivateKey,
    // pfCallerFreeProvOrNCryptKey will always be true in our case so
    // we don't need to check it.
    CryptReleaseContext(reinterpret_cast<HCRYPTPROV>(prov), 0);
  };
  std::unique_ptr<void, decltype(close_prov)> prov(
      reinterpret_cast<void*>(prov_raw), close_prov);

  // Get the private key from the provider.
  HCRYPTKEY pkey_raw;
  if (!CryptGetUserKey(prov_raw, pdwKeySpec, &pkey_raw)) {
    return InvalidArgumentError(
        FormatWin32Errors("No private key found in PKCS#12 file (", source,
                          "): "),
        GCP_ERROR_INFO());
  }
  auto close_pkey = [](void* pkey) {
    CryptDestroyKey(reinterpret_cast<HCRYPTKEY>(pkey));
  };
  std::unique_ptr<void, decltype(close_pkey)> pkey(
      reinterpret_cast<void*>(pkey_raw), close_pkey);

  // Export the private key to a blob.
  DWORD exported_key_length;
  if (!CryptExportKey(reinterpret_cast<HCRYPTKEY>(pkey.get()), 0,
                      PRIVATEKEYBLOB, 0, nullptr, &exported_key_length)) {
    return InvalidArgumentError(
        FormatWin32Errors("Could not export private key from PKCS#12 file (",
                          source, "): "),
        GCP_ERROR_INFO());
  }
  std::vector<char> exported_key(exported_key_length);
  CryptExportKey(reinterpret_cast<HCRYPTKEY>(pkey.get()), 0, PRIVATEKEYBLOB, 0,
                 reinterpret_cast<BYTE*>(exported_key.data()),
                 &exported_key_length);

  // Encode the blob to PKCS#1 format.
  DWORD pkcs1_encoded_length;
  if (!CryptEncodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                           PKCS_RSA_PRIVATE_KEY, exported_key.data(), 0,
                           nullptr, nullptr, &pkcs1_encoded_length)) {
    return InvalidArgumentError(
        FormatWin32Errors("Could not encode private key from PKCS#12 file (",
                          source, "): "),
        GCP_ERROR_INFO());
  }
  std::vector<BYTE> pkcs1_encoded(pkcs1_encoded_length);
  CryptEncodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                      PKCS_RSA_PRIVATE_KEY, exported_key.data(), 0, nullptr,
                      pkcs1_encoded.data(), &pkcs1_encoded_length);

  // Wrap the PKCS#1 encoded private key in a PKCS#8 structure.
  CRYPT_PRIVATE_KEY_INFO private_key_info;
  private_key_info.Version = 0;
  private_key_info.Algorithm.pszObjId = szOID_RSA_RSA;
  private_key_info.Algorithm.Parameters.cbData = 0;
  private_key_info.Algorithm.Parameters.pbData = nullptr;
  private_key_info.PrivateKey.cbData = pkcs1_encoded_length;
  private_key_info.PrivateKey.pbData = pkcs1_encoded.data();
  private_key_info.pAttributes = nullptr;
  DWORD pkcs8_encoded_length;
  if (!CryptEncodeObjectEx(X509_ASN_ENCODING, PKCS_PRIVATE_KEY_INFO,
                           &private_key_info, 0, nullptr, nullptr,
                           &pkcs8_encoded_length)) {
    return InvalidArgumentError(
        FormatWin32Errors("Could not encode private key from PKCS#12 file (",
                          source, "): "),
        GCP_ERROR_INFO());
  }
  std::vector<BYTE> pkcs8_encoded(pkcs8_encoded_length);
  CryptEncodeObjectEx(X509_ASN_ENCODING, PKCS_PRIVATE_KEY_INFO,
                      &private_key_info, 0, nullptr, pkcs8_encoded.data(),
                      &pkcs8_encoded_length);

  // Convert to base64 and add the PEM markers.
  DWORD base64_length;
  if (!CryptBinaryToStringA(pkcs8_encoded.data(), pkcs8_encoded_length,
                            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCR, nullptr,
                            &base64_length)) {
    return InvalidArgumentError(
        FormatWin32Errors(
            "Could not base64 encode private key from PKCS#12 file (", source,
            "): "),
        GCP_ERROR_INFO());
  }
  std::vector<char> private_key(base64_length);
  CryptBinaryToStringA(pkcs8_encoded.data(), pkcs8_encoded_length,
                       CRYPT_STRING_BASE64 | CRYPT_STRING_NOCR,
                       private_key.data(), &base64_length);
  private_key.pop_back();  // remove the null terminator

  return ServiceAccountCredentialsInfo{
      std::move(service_account_id),
      P12PrivateKeyIdMarker(),
      absl::StrCat("-----BEGIN PRIVATE KEY-----\n",
                   absl::string_view(private_key.data(), private_key.size()),
                   "-----END PRIVATE KEY-----\n"),
      GoogleOAuthRefreshEndpoint(),
      /*scopes=*/{},
      /*subject=*/{},
      /*enable_self_signed_jwt=*/false,
      /*universe_domain=*/{}};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
#endif
