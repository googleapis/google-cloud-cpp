// Copyright 2018 Google LLC
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

#include "google/cloud/internal/oauth2_service_account_credentials.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/make_jwt_assertion.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/oauth2_google_credentials.h"
#include "google/cloud/internal/oauth2_universe_domain.h"
#include "google/cloud/internal/openssl_util.h"
#include "google/cloud/internal/rest_response.h"
#include <nlohmann/json.hpp>
#ifdef _WIN32
#include <fstream>
#include <type_traits>
#include <vector>
#include <Windows.h>
#include <wincrypt.h>
#else
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#endif

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

auto constexpr kP12PrivateKeyIdMarker = "--unknown--";

}  // namespace

using ::google::cloud::internal::MakeJWTAssertionNoThrow;

StatusOr<ServiceAccountCredentialsInfo> ParseServiceAccountCredentials(
    std::string const& content, std::string const& source,
    std::string const& default_token_uri) {
  auto credentials = nlohmann::json::parse(content, nullptr, false);
  if (credentials.is_discarded()) {
    return internal::InvalidArgumentError(
        "Invalid ServiceAccountCredentials,"
        "parsing failed on data loaded from " +
        source);
  }
  std::string const private_key_id_key = "private_key_id";
  std::string const private_key_key = "private_key";
  std::string const token_uri_key = "token_uri";
  std::string const client_email_key = "client_email";
  std::string const universe_domain_key = "universe_domain";
  for (auto const& key : {private_key_key, client_email_key}) {
    if (credentials.count(key) == 0) {
      return internal::InvalidArgumentError(
          "Invalid ServiceAccountCredentials, the " + std::string(key) +
          " field is missing on data loaded from " + source);
    }
    if (credentials.value(key, "").empty()) {
      return internal::InvalidArgumentError(
          "Invalid ServiceAccountCredentials, the " + std::string(key) +
          " field is empty on data loaded from " + source);
    }
  }
  // The token_uri and universe_domain fields may be missing, but may not be
  // empty.
  for (auto const& key : {token_uri_key, universe_domain_key}) {
    if (credentials.count(key) != 0 && credentials.value(key, "").empty()) {
      return internal::InvalidArgumentError(
          "Invalid ServiceAccountCredentials, the " + std::string(key) +
          " field is empty on data loaded from " + source);
    }
  }
  return ServiceAccountCredentialsInfo{
      credentials.value(client_email_key, ""),
      credentials.value(private_key_id_key, ""),
      credentials.value(private_key_key, ""),
      // Some credential formats (e.g. gcloud's ADC file) don't contain a
      // "token_uri" attribute in the JSON object.  In this case, we try using
      // the default value.
      credentials.value(token_uri_key, default_token_uri),
      /*scopes=*/absl::nullopt,
      /*subject=*/absl::nullopt,
      /*enable_self_signed_jwt=*/true,
      credentials.value(universe_domain_key, GoogleDefaultUniverseDomain())};
}

std::pair<std::string, std::string> AssertionComponentsFromInfo(
    ServiceAccountCredentialsInfo const& info,
    std::chrono::system_clock::time_point now) {
  nlohmann::json assertion_header = {{"alg", "RS256"}, {"typ", "JWT"}};
  if (!info.private_key_id.empty()) {
    assertion_header["kid"] = info.private_key_id;
  }

  // Scopes must be specified in a space separated string:
  //    https://google.aip.dev/auth/4112
  auto scopes = [&info]() -> std::string {
    if (!info.scopes) return GoogleOAuthScopeCloudPlatform();
    return absl::StrJoin(*(info.scopes), " ");
  }();

  auto expiration = now + GoogleOAuthAccessTokenLifetime();
  // As much as possible, do the time arithmetic using the std::chrono types.
  // Convert to an integer only when we are dealing with timestamps since the
  // epoch. Note that we cannot use `time_t` directly because that might be a
  // floating point.
  auto const now_from_epoch =
      static_cast<std::intmax_t>(std::chrono::system_clock::to_time_t(now));
  auto const expiration_from_epoch = static_cast<std::intmax_t>(
      std::chrono::system_clock::to_time_t(expiration));
  nlohmann::json assertion_payload = {
      {"iss", info.client_email},
      {"scope", scopes},
      {"aud", info.token_uri},
      {"iat", now_from_epoch},
      // Resulting access token should expire after one hour.
      {"exp", expiration_from_epoch}};
  if (info.subject) {
    assertion_payload["sub"] = *(info.subject);
  }

  // Note: we don't move here as it would prevent copy elision.
  return std::make_pair(assertion_header.dump(), assertion_payload.dump());
}

std::string MakeJWTAssertion(std::string const& header,
                             std::string const& payload,
                             std::string const& pem_contents) {
  return internal::MakeJWTAssertionNoThrow(header, payload, pem_contents)
      .value();
}

std::vector<std::pair<std::string, std::string>>
CreateServiceAccountRefreshPayload(ServiceAccountCredentialsInfo const& info,
                                   std::chrono::system_clock::time_point now) {
  std::string header;
  std::string payload;
  std::tie(header, payload) = AssertionComponentsFromInfo(info, now);
  return {{"grant_type", "urn:ietf:params:oauth:grant-type:jwt-bearer"},
          {"assertion", MakeJWTAssertion(header, payload, info.private_key)}};
}

StatusOr<AccessToken> ParseServiceAccountRefreshResponse(
    rest_internal::RestResponse& response,
    std::chrono::system_clock::time_point now) {
  auto status_code = response.StatusCode();
  auto payload = rest_internal::ReadAll(std::move(response).ExtractPayload());
  if (!payload.ok()) return std::move(payload).status();
  auto access_token = nlohmann::json::parse(*payload, nullptr, false);
  if (access_token.is_discarded() || access_token.count("access_token") == 0 ||
      access_token.count("expires_in") == 0 ||
      access_token.count("token_type") == 0) {
    auto error_payload =
        *payload +
        "Could not find all required fields in response (access_token,"
        " expires_in, token_type) while trying to obtain an access token for"
        " service account credentials.";
    return AsStatus(status_code, error_payload);
  }

  auto expires_in = std::chrono::seconds(access_token.value("expires_in", 0));
  return AccessToken{access_token.value("access_token", ""), now + expires_in};
}

StatusOr<std::string> MakeSelfSignedJWT(
    ServiceAccountCredentialsInfo const& info,
    std::chrono::system_clock::time_point tp) {
  auto scope = [&info]() -> std::string {
    if (!info.scopes.has_value()) return GoogleOAuthScopeCloudPlatform();
    if (info.scopes->empty()) return GoogleOAuthScopeCloudPlatform();
    return absl::StrJoin(*info.scopes, " ");
  };

  auto const header = nlohmann::json{
      {"alg", "RS256"}, {"typ", "JWT"}, {"kid", info.private_key_id}};
  // As much as possible, do the time arithmetic using the std::chrono types.
  // Convert to an integer only when we are dealing with timestamps since the
  // epoch. Note that we cannot use `time_t` directly because that might be a
  // floating point.
  auto const expiration = tp + GoogleOAuthAccessTokenLifetime();
  auto const iat =
      static_cast<std::intmax_t>(std::chrono::system_clock::to_time_t(tp));
  auto const exp = static_cast<std::intmax_t>(
      std::chrono::system_clock::to_time_t(expiration));
  auto payload = nlohmann::json{
      {"iss", info.client_email},
      {"sub", info.client_email},
      {"iat", iat},
      {"exp", exp},
      {"scope", scope()},
  };

  return MakeJWTAssertionNoThrow(header.dump(), payload.dump(),
                                 info.private_key);
}

ServiceAccountCredentials::ServiceAccountCredentials(
    ServiceAccountCredentialsInfo info, Options options,
    HttpClientFactory client_factory)
    : info_(std::move(info)),
      options_(internal::MergeOptions(
          std::move(options),
          Options{}.set<ServiceAccountCredentialsTokenUriOption>(
              info_.token_uri))),
      client_factory_(std::move(client_factory)) {}

StatusOr<AccessToken> ServiceAccountCredentials::GetToken(
    std::chrono::system_clock::time_point tp) {
  if (UseOAuth()) return GetTokenOAuth(tp);
  return GetTokenSelfSigned(tp);
}

StatusOr<std::vector<std::uint8_t>> ServiceAccountCredentials::SignBlob(
    absl::optional<std::string> const& signing_account,
    std::string const& blob) const {
  if (signing_account.has_value() &&
      signing_account.value() != info_.client_email) {
    return Status(StatusCode::kInvalidArgument,
                  "The current_credentials cannot sign blobs for " +
                      signing_account.value());
  }
  return internal::SignUsingSha256(blob, info_.private_key);
}

StatusOr<std::string> ServiceAccountCredentials::universe_domain() const {
  if (!info_.universe_domain.has_value()) {
    return internal::NotFoundError(
        "universe_domain is not present in the credentials");
  }
  return *info_.universe_domain;
}

StatusOr<std::string> ServiceAccountCredentials::universe_domain(
    Options const&) const {
  // universe_domain is stored locally, so any retry options are unnecessary.
  return universe_domain();
}

#ifdef _WIN32
StatusOr<ServiceAccountCredentialsInfo> ParseServiceAccountP12File(
    std::string const& source) {
  // Read the PKCS#12 file into memory.
  std::vector<char> data;
  {
    std::ifstream file(source, std::ios::binary);
    if (!file.is_open()) {
      return Status(StatusCode::kInvalidArgument,
                    "Cannot open PKCS#12 file (" + source + ")");
    }
    data.assign(std::istreambuf_iterator<char>{file}, {});
  }

  auto capture_win32_errors = []() {
    auto last_error = GetLastError();
    LPSTR message_buffer_raw = nullptr;
    auto size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, last_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&message_buffer_raw, 0, nullptr);
    std::unique_ptr<char, decltype(&LocalFree)> message_buffer(
        message_buffer_raw, &LocalFree);
    return std::string(message_buffer.get(), size) + " (error code " +
           std::to_string(last_error) + ")";
  };

  // Import the PKCS#12 file into a certificate store.
  HCERTSTORE certstore_raw = [data = std::move(data)]() mutable {
    CRYPT_DATA_BLOB dataBlob = {static_cast<DWORD>(data.size()),
                                reinterpret_cast<BYTE*>(data.data())};
    return PFXImportCertStore(&dataBlob, L"notasecret", CRYPT_EXPORTABLE);
  }();
  if (certstore_raw == nullptr) {
    auto last_error = GetLastError();
    std::string msg = "Cannot parse PKCS#12 file (" + source + "): ";
    msg += capture_win32_errors();
    return Status(StatusCode::kInvalidArgument, msg);
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
    return Status(StatusCode::kInvalidArgument,
                  "No certificate found in PKCS#12 file (" + source + ")");
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
    return Status(
        StatusCode::kInvalidArgument,
        "Invalid PKCS#12 file (" + source +
            "): service account id missing or not not formatted correctly");
  }

  // Get a provider that has the private key of the certificate.
  HCRYPTPROV prov_raw;
  DWORD pdwKeySpec;
  BOOL pfCallerFreeProvOrNCryptKey;
  if (!CryptAcquireCertificatePrivateKey(cert.get(), CRYPT_ACQUIRE_SILENT_FLAG,
                                         nullptr, &prov_raw, &pdwKeySpec,
                                         &pfCallerFreeProvOrNCryptKey)) {
    std::string msg = "No private key found in PKCS#12 file (" + source + "): ";
    msg += capture_win32_errors();
    return Status(StatusCode::kInvalidArgument, msg);
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
    std::string msg = "No private key found in PKCS#12 file (" + source + "): ";
    msg += capture_win32_errors();
    return Status(StatusCode::kInvalidArgument, msg);
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
    std::string msg =
        "Could not export private key from PKCS#12 file (" + source + "): ";
    msg += capture_win32_errors();
    return Status(StatusCode::kInvalidArgument, msg);
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
    std::string msg =
        "Could not encode private key from PKCS#12 file (" + source + "): ";
    msg += capture_win32_errors();
    return Status(StatusCode::kInvalidArgument, msg);
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
    std::string msg =
        "Could not encode private key from PKCS#12 file (" + source + "): ";
    msg += capture_win32_errors();
    return Status(StatusCode::kInvalidArgument, msg);
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
    std::string msg =
        "Could not base64 encode private key from PKCS#12 file (" + source +
        "): ";
    msg += capture_win32_errors();
    return Status(StatusCode::kInvalidArgument, msg);
  }
  std::string private_key = "-----BEGIN PRIVATE KEY-----\n";
  // Enlarge the string and directly write the base64 data into it.
  private_key.resize(private_key.size() + base64_length);
  CryptBinaryToStringA(pkcs8_encoded.data(), pkcs8_encoded_length,
                       CRYPT_STRING_BASE64 | CRYPT_STRING_NOCR,
                       &*(private_key.end() - base64_length), &base64_length);
  private_key.pop_back();  // remove the null terminator
  private_key += "-----END PRIVATE KEY-----\n";

  return ServiceAccountCredentialsInfo{std::move(service_account_id),
                                       kP12PrivateKeyIdMarker,
                                       std::move(private_key),
                                       GoogleOAuthRefreshEndpoint(),
                                       /*scopes=*/{},
                                       /*subject=*/{},
                                       /*enable_self_signed_jwt=*/false,
                                       /*universe_domain=*/{}};
}
#else
StatusOr<ServiceAccountCredentialsInfo> ParseServiceAccountP12File(
    std::string const& source) {
  OpenSSL_add_all_algorithms();

  PKCS12* p12_raw = [](std::string const& source) {
    auto bio = std::unique_ptr<BIO, decltype(&BIO_free)>(
        BIO_new_file(source.c_str(), "rb"), &BIO_free);
    if (!bio) return static_cast<PKCS12*>(nullptr);
    return d2i_PKCS12_bio(bio.get(), nullptr);
  }(source);

  std::unique_ptr<PKCS12, decltype(&PKCS12_free)> p12(p12_raw, &PKCS12_free);

  auto capture_openssl_errors = []() {
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
  };

  if (p12 == nullptr) {
    std::string msg = "Cannot open PKCS#12 file (" + source + "): ";
    msg += capture_openssl_errors();
    return Status(StatusCode::kInvalidArgument, msg);
  }

  EVP_PKEY* pkey_raw;
  X509* cert_raw;
  if (PKCS12_parse(p12.get(), "notasecret", &pkey_raw, &cert_raw, nullptr) !=
      1) {
    std::string msg = "Cannot parse PKCS#12 file (" + source + "): ";
    msg += capture_openssl_errors();
    return Status(StatusCode::kInvalidArgument, msg);
  }

  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey(pkey_raw,
                                                           &EVP_PKEY_free);
  std::unique_ptr<X509, decltype(&X509_free)> cert(cert_raw, &X509_free);

  if (pkey_raw == nullptr) {
    return Status(StatusCode::kInvalidArgument,
                  "No private key found in PKCS#12 file (" + source + ")");
  }
  if (cert_raw == nullptr) {
    return Status(StatusCode::kInvalidArgument,
                  "No certificate found in PKCS#12 file (" + source + ")");
  }

  // This is automatically deleted by `cert`.
  X509_NAME* name = X509_get_subject_name(cert.get());

  std::string service_account_id = [&name]() -> std::string {
    auto openssl_free = [](void* addr) { OPENSSL_free(addr); };
    std::unique_ptr<char, decltype(openssl_free)> oneline(
        X509_NAME_oneline(name, nullptr, 0), openssl_free);
    // We expect the name to be simply CN/ followed by a (small) number of
    // digits.
    if (strncmp("/CN=", oneline.get(), 4) != 0) {
      return "";
    }
    return oneline.get() + 4;
  }();

  if (service_account_id.find_first_not_of("0123456789") != std::string::npos ||
      service_account_id.empty()) {
    return Status(
        StatusCode::kInvalidArgument,
        "Invalid PKCS#12 file (" + source +
            "): service account id missing or not not formatted correctly");
  }

  std::unique_ptr<BIO, decltype(&BIO_free)> mem_io(BIO_new(BIO_s_mem()),
                                                   &BIO_free);

  if (PEM_write_bio_PKCS8PrivateKey(mem_io.get(), pkey.get(), nullptr, nullptr,
                                    0, nullptr, nullptr) == 0) {
    std::string msg =
        "Cannot print private key in PKCS#12 file (" + source + "): ";
    msg += capture_openssl_errors();
    return Status(StatusCode::kUnknown, msg);
  }

  // This buffer belongs to the BIO chain and is freed upon its destruction.
  BUF_MEM* buf_mem;
  BIO_get_mem_ptr(mem_io.get(), &buf_mem);

  std::string private_key(buf_mem->data, buf_mem->length);

  return ServiceAccountCredentialsInfo{std::move(service_account_id),
                                       kP12PrivateKeyIdMarker,
                                       std::move(private_key),
                                       GoogleOAuthRefreshEndpoint(),
                                       /*scopes=*/{},
                                       /*subject=*/{},
                                       /*enable_self_signed_jwt=*/false,
                                       /*universe_domain=*/{}};
}
#endif

bool ServiceAccountUseOAuth(ServiceAccountCredentialsInfo const& info) {
  // Custom universe domains are only supported with JWT, not OAuth tokens.
  if (info.universe_domain.has_value() &&
      info.universe_domain != GoogleDefaultUniverseDomain()) {
    return false;
  }
  if (info.private_key_id == kP12PrivateKeyIdMarker ||
      !info.enable_self_signed_jwt) {
    return true;
  }
  auto disable_jwt = google::cloud::internal::GetEnv(
      "GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT");
  return disable_jwt.has_value();
}

bool ServiceAccountCredentials::UseOAuth() {
  return ServiceAccountUseOAuth(info_);
}

StatusOr<AccessToken> ServiceAccountCredentials::GetTokenOAuth(
    std::chrono::system_clock::time_point tp) const {
  auto client = client_factory_(options_);
  rest_internal::RestRequest request;
  request.SetPath(options_.get<ServiceAccountCredentialsTokenUriOption>());
  auto payload = CreateServiceAccountRefreshPayload(info_, tp);
  rest_internal::RestContext context;
  auto response = client->Post(context, request, payload);
  if (!response) return std::move(response).status();
  if (IsHttpError(**response)) return AsStatus(std::move(**response));
  return ParseServiceAccountRefreshResponse(**response, tp);
}

StatusOr<AccessToken> ServiceAccountCredentials::GetTokenSelfSigned(
    std::chrono::system_clock::time_point tp) const {
  auto token = MakeSelfSignedJWT(info_, tp);
  if (!token) return std::move(token).status();
  return AccessToken{*token, tp + GoogleOAuthAccessTokenLifetime()};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
