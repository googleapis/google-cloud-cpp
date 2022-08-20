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

#include "google/cloud/internal/oauth2_google_credentials.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/make_jwt_assertion.h"
#include "google/cloud/internal/oauth2_authorized_user_credentials.h"
#include "google/cloud/internal/oauth2_compute_engine_credentials.h"
#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/internal/oauth2_google_application_default_credentials_file.h"
#include "google/cloud/internal/oauth2_service_account_credentials.h"
#include "google/cloud/internal/throw_delegate.h"
#include "absl/memory/memory.h"
#include <nlohmann/json.hpp>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <fstream>
#include <iterator>
#include <memory>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

auto constexpr kP12PrivateKeyIdMarker = "--unknown--";

#include "google/cloud/internal/disable_msvc_crt_secure_warnings.inc"
StatusOr<ServiceAccountCredentialsInfo> ParseServiceAccountP12File(
    std::string const& source) {
  OpenSSL_add_all_algorithms();

  PKCS12* p12_raw = [](std::string const& source) {
    FILE* fp = std::fopen(source.c_str(), "rb");
    if (fp == nullptr) {
      return static_cast<PKCS12*>(nullptr);
    }
    auto* result = d2i_PKCS12_fp(fp, nullptr);
    fclose(fp);
    return result;
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
                  "No private key found in PKCS#12 file (" + source + ")");
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
                                       /*scopes*/ {},
                                       /*subject*/ {}};
}
#include "google/cloud/internal/diagnostics_pop.inc"

// Parses the JSON at `path` and creates the appropriate
// Credentials type.
//
// If `service_account_scopes` or `service_account_subject` are specified, the
// file at `path` must be a JSON service account. If a different type of
// credential file is found, this function returns nullptr to indicate a service
// account file wasn't found.
StatusOr<std::unique_ptr<Credentials>> LoadCredsFromPath(
    std::string const& path, Options const& options) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    // We use kUnknown here because we don't know if the file does not exist, or
    // if we were unable to open it for some other reason.
    return Status(StatusCode::kUnknown, "Cannot open credentials file " + path);
  }
  std::string contents(std::istreambuf_iterator<char>{ifs}, {});
  auto cred_json = nlohmann::json::parse(contents, nullptr, false);
  if (!cred_json.is_object()) {
    // This is not a JSON file, try to load it as a P12 service account.
    auto info = ParseServiceAccountP12File(path);
    if (!info) {
      return Status(
          StatusCode::kInvalidArgument,
          "Cannot open credentials file " + path +
              ", it does not contain a JSON object, nor can be parsed "
              "as a PKCS#12 file. " +
              info.status().message());
    }
    info->scopes = {};
    info->subject = {};
    auto credentials = absl::make_unique<ServiceAccountCredentials>(*info);
    return std::unique_ptr<Credentials>(std::move(credentials));
  }
  std::string cred_type = cred_json.value("type", "no type given");
  // If non_service_account_ok==false and the cred_type is authorized_user,
  // we'll return "Unsupported credential type (authorized_user)".
  if (cred_type == "authorized_user") {
    auto info = ParseAuthorizedUserCredentials(contents, path);
    if (!info) return std::move(info).status();
    std::unique_ptr<Credentials> ptr =
        absl::make_unique<AuthorizedUserCredentials>(*info);
    return StatusOr<std::unique_ptr<Credentials>>(std::move(ptr));
  }
  if (cred_type == "service_account") {
    auto info = ParseServiceAccountCredentials(contents, path);
    if (!info) return std::move(info).status();
    std::unique_ptr<Credentials> ptr =
        absl::make_unique<ServiceAccountCredentials>(*info, options);
    return StatusOr<std::unique_ptr<Credentials>>(std::move(ptr));
  }
  return StatusOr<std::unique_ptr<Credentials>>(
      Status(StatusCode::kInvalidArgument,
             "Unsupported credential type (" + cred_type +
                 ") when reading Application Default Credentials file from " +
                 path + "."));
}

// Tries to load the file at the path specified by the value of the Application
// Default %Credentials environment variable and to create the appropriate
// Credentials type.
//
// Returns nullptr if the environment variable is not set or the path does not
// exist.
//
// If `service_account_scopes` or `service_account_subject` are specified, the
// found file must be a JSON service account. If a different type of credential
// file is found, this function returns nullptr to indicate a service account
// file wasn't found.
StatusOr<std::unique_ptr<Credentials>> MaybeLoadCredsFromAdcPaths(
    Options const& options = {}) {
  // 1) Check if the GOOGLE_APPLICATION_CREDENTIALS environment variable is set.
  auto path = GoogleAdcFilePathFromEnvVarOrEmpty();
  if (path.empty()) {
    // 2) If no path was specified via environment variable, check if the
    // gcloud ADC file exists.
    path = GoogleAdcFilePathFromWellKnownPathOrEmpty();
    if (path.empty()) {
      return StatusOr<std::unique_ptr<Credentials>>(nullptr);
    }
    // Just because we had the necessary information to build the path doesn't
    // mean that a file exists there.
    std::error_code ec;
    auto adc_file_status = google::cloud::internal::status(path, ec);
    if (!google::cloud::internal::exists(adc_file_status)) {
      return StatusOr<std::unique_ptr<Credentials>>(nullptr);
    }
  }

  // If the path was specified, try to load that file; explicitly fail if it
  // doesn't exist or can't be read and parsed.
  return LoadCredsFromPath(path, options);
}

}  // namespace

StatusOr<std::shared_ptr<Credentials>> GoogleDefaultCredentials(
    Options const& options) {
  // 1 and 2) Check if the GOOGLE_APPLICATION_CREDENTIALS environment variable
  // is set or if the gcloud ADC file exists.
  auto creds = MaybeLoadCredsFromAdcPaths(options);
  if (!creds) return std::move(creds).status();
  if (*creds) return std::shared_ptr<Credentials>(*std::move(creds));

  // 3) Check for implicit environment-based credentials (GCE, GAE Flexible,
  // Cloud Run or GKE Environment).
  return std::shared_ptr<Credentials>(
      std::make_shared<ComputeEngineCredentials>());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
