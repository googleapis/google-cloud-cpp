// Copyright 2025 Google LLC
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

#include "google/cloud/mtls_credentials_config.h"
#include <string>

namespace google {
namespace cloud {

MtlsCredentialsConfig::Rest::Rest() {}  // NOLINT(modernize-use-equals-default)

MtlsCredentialsConfig::Rest::Rest(std::string ssl_client_cert_filename)
    : ssl_client_cert_filename_(std::move(ssl_client_cert_filename)) {}
MtlsCredentialsConfig::Rest::Rest(std::string ssl_client_cert_filename,
                                  std::string ssl_key_file,
                                  std::string ssl_key_file_password)
    : ssl_client_cert_filename_(std::move(ssl_client_cert_filename)),
      ssl_key_file_(SslKeyFile{std::move(ssl_key_file),
                               std::move(ssl_key_file_password)}) {}

std::string MtlsCredentialsConfig::Rest::ssl_client_cert_filename() const {
  return ssl_client_cert_filename_;
}

absl::optional<std::string> MtlsCredentialsConfig::Rest::ssl_key_filename()
    const {
  if (ssl_key_file_.has_value()) return ssl_key_file_->ssl_key_filename;
  return absl::nullopt;
}

absl::optional<std::string> MtlsCredentialsConfig::Rest::ssl_key_file_password()
    const {
  if (ssl_key_file_.has_value()) return ssl_key_file_->ssl_key_file_password;
  return absl::nullopt;
}

MtlsCredentialsConfig::Rest::SslCertType
MtlsCredentialsConfig::Rest::ssl_cert_type() const {
  return ssl_cert_type_;
}

MtlsCredentialsConfig::Rest& MtlsCredentialsConfig::Rest::set_cert_type(
    SslCertType ssl_cert_type) {
  ssl_cert_type_ = ssl_cert_type;
  return *this;
}

std::string MtlsCredentialsConfig::Rest::ToString(SslCertType type) {
  switch (type) {
    case SslCertType::kPEM:
      return "PEM";
    case SslCertType::kDER:
      return "DER";
    case SslCertType::kP12:
      return "P12";
  }
  return "";
}

}  // namespace cloud
}  // namespace google
