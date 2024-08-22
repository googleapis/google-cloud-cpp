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
#include "google/cloud/internal/parse_service_account_p12_file.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/win32/win32_helpers.h"
#include "absl/types/span.h"
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

namespace {

struct HCertStoreDeleter {
  void operator()(HCERTSTORE store) const { CertCloseStore(store, 0); }
};

using UniqueCertStore =
    std::unique_ptr<std::remove_pointer_t<HCERTSTORE>, HCertStoreDeleter>;

struct CertContextDeleter {
  void operator()(PCCERT_CONTEXT cert) const {
    CertFreeCertificateContext(cert);
  }
};

using UniqueCertContext =
    std::unique_ptr<std::remove_pointer_t<PCCERT_CONTEXT>, CertContextDeleter>;

/// A wrapper around `std::unique_ptr` that has a get() method that returns a
/// native integer type. This is needed because some Windows API handles are
/// declared as native integers instead of pointers.
template <typename T, typename Deleter>
class UniquePtrReinterpretProxy {
  static_assert(sizeof(T) == sizeof(void*),
                "T must be the same size as a pointer");

 public:
  explicit UniquePtrReinterpretProxy(T ptr)
      : ptr_(reinterpret_cast<void*>(ptr), {}) {}

  T get() const { return reinterpret_cast<T>(ptr_.get()); }

 private:
  std::unique_ptr<void, Deleter> ptr_;
};

struct HCryptKeyDeleter {
  void operator()(void* key) const {
    CryptDestroyKey(reinterpret_cast<HCRYPTKEY>(key));
  }
};

using UniqueCryptKey = UniquePtrReinterpretProxy<HCRYPTKEY, HCryptKeyDeleter>;

StatusOr<UniqueCertStore> OpenP12File(std::string const& source) {
  // Read the PKCS#12 file into memory.
  std::vector<BYTE> data;
  {
    std::ifstream file(source, std::ios::binary);
    if (!file.is_open()) {
      return InvalidArgumentError(
          absl::StrCat("Cannot open PKCS#12 file (", source, ")"),
          GCP_ERROR_INFO());
    }
    data.assign(std::istreambuf_iterator<char>{file}, {});
    if (file.bad()) {
      return InvalidArgumentError(
          absl::StrCat("Cannot read PKCS#12 file (", source, ")"),
          GCP_ERROR_INFO());
    }
  }
  // DWORD is 32-bits big while size_t can have a size of 64 bits.
  // Both types are unsigned, which means that casting will not be
  // undefined behavior if the size is > 4GB. Such huge p12 files
  // should not practically exist, and if encountered the OS will
  // get a truncated view of them, and fail to read them.
  CRYPT_DATA_BLOB dataBlob = {static_cast<DWORD>(data.size()), data.data()};
  // Import the PKCS#12 file into a certificate store.
  HCERTSTORE certstore_raw = PFXImportCertStore(
      &dataBlob, L"notasecret", CRYPT_EXPORTABLE | PKCS12_NO_PERSIST_KEY);
  if (certstore_raw == nullptr) {
    return InvalidArgumentError(
        FormatWin32Errors("Cannot parse PKCS#12 file (", source, "): "),
        GCP_ERROR_INFO());
  }
  return UniqueCertStore(certstore_raw);
}

StatusOr<UniqueCertContext> GetCertificate(HCERTSTORE certstore,
                                           std::string const& source) {
  // Get the certificate from the store.
  PCCERT_CONTEXT cert_raw = CertEnumCertificatesInStore(certstore, nullptr);
  if (cert_raw == nullptr) {
    return InvalidArgumentError(
        absl::StrCat("No certificate found in PKCS#12 file (", source, ")"),
        GCP_ERROR_INFO());
  }
  return UniqueCertContext(cert_raw);
}

std::string GetCertificateCommonName(PCCERT_CONTEXT cert) {
  DWORD size = CertGetNameStringA(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0,
                                  nullptr, nullptr, 0);
  std::string result(size, '\0');
  CertGetNameStringA(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr,
                     &result[0], size);
  result.pop_back();  // remove the null terminator
  return result;
}

StatusOr<HCRYPTPROV> GetCertificatePrivateKey(PCCERT_CONTEXT cert,
                                              DWORD& dwKeySpec,
                                              std::string const& source) {
  DWORD context_length;
  if (!CertGetCertificateContextProperty(cert, CERT_KEY_CONTEXT_PROP_ID,
                                         nullptr, &context_length)) {
    return InvalidArgumentError(
        FormatWin32Errors("No private key found in PKCS#12 file (", source,
                          "): "),
        GCP_ERROR_INFO());
  }
  std::vector<BYTE> buffer(context_length);
  CertGetCertificateContextProperty(cert, CERT_KEY_CONTEXT_PROP_ID,
                                    buffer.data(), &context_length);
  auto& context = *reinterpret_cast<CERT_KEY_CONTEXT*>(buffer.data());
  dwKeySpec = context.dwKeySpec;
  // Documentation says that if we use PKCS12_NO_PERSIST_KEY, the key will
  // always be an NCRYPT_KEY_HANDLE. However it was observed that this is not
  // the case (https://github.com/MicrosoftDocs/sdk-api/pull/1874).
  assert(dwKeySpec != CERT_NCRYPT_KEY_SPEC);
  // Don't free the provider, its lifetime is controlled by the certificate
  // context (https://github.com/dotnet/corefx/pull/12010).
  return context.hCryptProv;
}

StatusOr<UniqueCryptKey> GetKeyFromProvider(HCRYPTPROV prov, DWORD dwKeySpec,
                                            std::string const& source) {
  HCRYPTKEY pkey_raw;
  if (!CryptGetUserKey(prov, dwKeySpec, &pkey_raw)) {
    return InvalidArgumentError(
        FormatWin32Errors("No private key found in PKCS#12 file (", source,
                          "): "),
        GCP_ERROR_INFO());
  }
  return UniqueCryptKey(pkey_raw);
}

StatusOr<std::vector<BYTE>> ExportPrivateKey(HCRYPTKEY pkey,
                                             std::string const& source) {
  DWORD exported_key_length;
  if (!CryptExportKey(pkey, 0, PRIVATEKEYBLOB, 0, nullptr,
                      &exported_key_length)) {
    return InvalidArgumentError(
        FormatWin32Errors("Could not export private key from PKCS#12 file (",
                          source, "): "),
        GCP_ERROR_INFO());
  }
  std::vector<BYTE> exported_key(exported_key_length);
  // We don't have to check again for errors; we already did in the previous
  // call to get the buffer size. Same with calls to CryptEncodeObjectEx and
  // CryptBinaryToStringA below.
  CryptExportKey(pkey, 0, PRIVATEKEYBLOB, 0, exported_key.data(),
                 &exported_key_length);
  return exported_key;
}

StatusOr<std::vector<BYTE>> EncodeRsaPrivateKey(
    absl::Span<BYTE const> exported_key, std::string const& source) {
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
  return pkcs1_encoded;
}

StatusOr<std::vector<BYTE>> EncodeRsaPkcs8PrivateKey(
    absl::Span<BYTE const> pkcs1_encoded, std::string const& source) {
  CRYPT_PRIVATE_KEY_INFO private_key_info;
  private_key_info.Version = 0;
  private_key_info.Algorithm.pszObjId = const_cast<LPSTR>(szOID_RSA_RSA);
  private_key_info.Algorithm.Parameters.cbData = 0;
  private_key_info.Algorithm.Parameters.pbData = nullptr;
  private_key_info.PrivateKey.cbData = static_cast<DWORD>(pkcs1_encoded.size());
  private_key_info.PrivateKey.pbData = const_cast<BYTE*>(pkcs1_encoded.data());
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
  return pkcs8_encoded;
}

StatusOr<std::vector<char>> Base64Encode(absl::Span<BYTE const> blob,
                                         std::string const& source) {
  DWORD base64_length;
  if (!CryptBinaryToStringA(blob.data(), static_cast<DWORD>(blob.size()),
                            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCR, nullptr,
                            &base64_length)) {
    return InvalidArgumentError(
        FormatWin32Errors(
            "Could not base64 encode private key from PKCS#12 file (", source,
            "): "),
        GCP_ERROR_INFO());
  }
  std::vector<char> private_key(base64_length);
  CryptBinaryToStringA(blob.data(), static_cast<DWORD>(blob.size()),
                       CRYPT_STRING_BASE64 | CRYPT_STRING_NOCR,
                       private_key.data(), &base64_length);
  private_key.pop_back();  // remove the null terminator
  return private_key;
}

}  // namespace

StatusOr<ServiceAccountCredentialsInfo> ParseServiceAccountP12File(
    std::string const& source) {
  // Open the PKCS#12 file.
  auto certstore = OpenP12File(source);
  if (!certstore) return std::move(certstore).status();

  // Get the certificate from the store.
  auto cert = GetCertificate(certstore->get(), source);
  if (!cert) return std::move(cert).status();

  // Get the service account ID from the certificate's common name.
  std::string service_account_id = GetCertificateCommonName(cert->get());

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
  // Make sure that the provider outlives the HCRYPTKEY.
  DWORD dwKeySpec;
  auto prov = GetCertificatePrivateKey(cert->get(), dwKeySpec, source);
  if (!prov) return std::move(prov).status();

  // Get the private key from the provider.
  auto pkey = GetKeyFromProvider(*prov, dwKeySpec, source);
  if (!pkey) return std::move(pkey).status();

  // Export the private key from the certificate to a blob.
  auto exported_key = ExportPrivateKey(pkey->get(), source);
  if (!exported_key) return std::move(exported_key).status();

  // Encode the blob to PKCS#1 format.
  auto pkcs1_encoded = EncodeRsaPrivateKey(*exported_key, source);
  if (!pkcs1_encoded) return std::move(pkcs1_encoded).status();

  // Wrap the PKCS#1 encoded private key in a PKCS#8 structure.
  auto pkcs8_encoded = EncodeRsaPkcs8PrivateKey(*pkcs1_encoded, source);
  if (!pkcs8_encoded) return std::move(pkcs8_encoded).status();

  // Convert to base64.
  auto private_key = Base64Encode(*pkcs8_encoded, source);
  if (!private_key) return std::move(private_key).status();

  return ServiceAccountCredentialsInfo{
      std::move(service_account_id),
      P12PrivateKeyIdMarker(),
      absl::StrCat("-----BEGIN PRIVATE KEY-----\n",
                   absl::string_view(private_key->data(), private_key->size()),
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
