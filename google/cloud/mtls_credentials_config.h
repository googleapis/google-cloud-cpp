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
  struct Rest {
    enum class SslCertType { kPEM = 0, kDER, kP12 };

    /// https://curl.se/libcurl/c/CURLOPT_SSLCERT.html
    std::string ssl_client_cert_file;

    /// https://curl.se/libcurl/c/CURLOPT_SSLKEY.html
    std::string ssl_key_file;

    /// https://curl.se/libcurl/c/CURLOPT_KEYPASSWD.html
    std::string ssl_key_file_password;

    /// https://curl.se/libcurl/c/CURLOPT_SSLCERTTYPE.html
    SslCertType ssl_cert_type;
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
