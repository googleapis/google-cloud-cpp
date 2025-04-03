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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SSL_CERTIFICATE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SSL_CERTIFICATE_H

#include "google/cloud/version.h"
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace experimental {

/**
 * Represents an SSL certificate used in TLS authentication.
 */
class SslCertificate {
 public:
  enum class SslCertificateType { kPEM, kDER, kP12 };

  /// Creates and empty certificate.
  SslCertificate() = default;

  /// Creates a PEM certificate from the values provided.
  SslCertificate(std::string ssl_certificate, std::string ssl_private_key)
      : ssl_certificate_(std::move(ssl_certificate)),
        ssl_private_key_(std::move(ssl_private_key)) {}

  /// Creates a user specified type of certificate from the values provided.
  SslCertificate(std::string ssl_certificate, std::string ssl_private_key,
                 SslCertificateType ssl_certificate_type)
      : ssl_certificate_(std::move(ssl_certificate)),
        ssl_private_key_(std::move(ssl_private_key)),
        ssl_certificate_type_(ssl_certificate_type) {}

  std::string const& ssl_certificate() const { return ssl_certificate_; }

  std::string const& ssl_private_key() const { return ssl_private_key_; }

  SslCertificateType ssl_certificate_type() const {
    return ssl_certificate_type_;
  }

  static std::string ToString(SslCertificateType type) {
    switch (type) {
      case SslCertificateType::kPEM:
        return "PEM";
      case SslCertificateType::kDER:
        return "DER";
      case SslCertificateType::kP12:
        return "P12";
    }
    return {};
  }

 private:
  std::string ssl_certificate_;
  std::string ssl_private_key_;
  SslCertificateType ssl_certificate_type_ = SslCertificateType::kPEM;
};

}  // namespace experimental
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SSL_CERTIFICATE_H
