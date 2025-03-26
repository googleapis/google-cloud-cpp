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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_MTLS_CREDENTIALS_CONFIG_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_MTLS_CREDENTIALS_CONFIG_H

#include "google/cloud/common_options.h"
#include "google/cloud/experimental_tag.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include "absl/types/variant.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct MtlsCredentialsConfig {
  /**
   * Stores the client SSL certificate along with any other needed values to
   * access the certificate.
   *
   * The data in this class is used to set various options in libcurl:
   *  - ssl_client_cert_filename: CURLOPT_SSLCERT
   *  - ssl_key_filename: CURLOPT_SSLKEY
   *  - ssl_key_file_password: CURLOPT_KEYPASSWD
   *  - ssl_cert_type: CURLOPT_SSLCERTTYPE - defaults to PEM
   *
   *  Please see https://curl.se/libcurl/c/easy_setopt_options.html for more
   *  detailed information on the behavior of setting these options.
   *
   *  Additionally, you may need to set
   *   - google::cloud::CARootsFilePathOption to modify CURLOPT_CAINFO and/or
   *   - google::cloud::CAPathOption to modify CURLOPT_CAPATH
   *  if you're certificates are not in the system default location.
   *
   */
  class Rest {
   public:
    enum class SslCertType { kPEM, kDER, kP12 };

    Rest();
    explicit Rest(std::string ssl_client_cert_filename);
    Rest(std::string ssl_client_cert, std::string ssl_key_filename,
         std::string ssl_key_file_password);

    std::string ssl_client_cert_filename() const;
    absl::optional<std::string> ssl_key_filename() const;
    absl::optional<std::string> ssl_key_file_password() const;
    SslCertType ssl_cert_type() const;

    Rest& set_cert_type(SslCertType ssl_cert_type);

    static std::string ToString(SslCertType type);

   private:
    struct SslKeyFile {
      std::string ssl_key_filename;
      std::string ssl_key_file_password;
    };
    std::string ssl_client_cert_filename_;
    absl::optional<SslKeyFile> ssl_key_file_ = absl::nullopt;
    SslCertType ssl_cert_type_ = SslCertType::kPEM;
  };

  struct gRpc {
    /// gRPC only supports PEM files.
    enum class SslCertType {
      kPEM = 0,
    };

    /// The buffer containing the PEM encoding of the server root certificates.
    /// If this parameter is empty, the default roots will be used.  The default
    /// roots can be overridden using the GRPC_DEFAULT_SSL_ROOTS_FILE_PATH
    /// environment variable pointing to a file on the file system containing
    /// the roots.
    std::string pem_root_certs;

    /// The buffer containing the PEM encoding of the client's private key. This
    /// parameter can be empty if the client does not have a private key.
    std::string pem_private_key;

    /// The buffer containing the PEM encoding of the client's certificate
    /// chain. This parameter can be empty if the client does not have a
    /// certificate chain.
    std::string pem_cert_chain;
  };

  absl::variant<Rest, gRpc> config;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_MTLS_CREDENTIALS_CONFIG_H
