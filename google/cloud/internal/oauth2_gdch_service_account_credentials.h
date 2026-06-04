// Copyright 2026 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_GDCH_SERVICE_ACCOUNT_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_GDCH_SERVICE_ACCOUNT_CREDENTIALS_H

#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/internal/oauth2_http_client_factory.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <nlohmann/json_fwd.hpp>
#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Implements GDCH service account credentials for REST clients.
 *
 * This class is not intended for use by application developers. But it is
 * sufficiently complex that it deserves documentation for library developers.
 *
 * This class description assumes that you are familiar with [service accounts],
 * and [service account keys].
 *
 * The various `CreqteFrom*` methods parse the contents of the JSON key file. If
 * the key is parsed successfully, an instance of this class is created. The
 * service account key is never sent. Instead, this class creates a self-signed
 * JWT from the contents of the JSON key file and uses it as the subject_token
 * to perform an exchange via the token_uri found in the JSON key file to get an
 * access_token.
 */
class GDCHServiceAccountCredentials : public Credentials {
 public:
  /// Object to hold information used to instantiate an
  /// GDCHServiceAccountCredentials.
  struct Info {
    // From json file
    std::string project_id;
    std::string private_key_id;
    std::string private_key;
    std::string service_identity_name;
    std::string ca_cert_path;
    std::string token_uri;

    // Additional data provided by the user.
    std::string audience;
  };

  /// Parses the contents of a JSON keyfile into a
  /// GDCHServiceAccountCredentialsInfo.
  static StatusOr<Info> Parse(std::string const& content,
                              std::string const& source);

  /// Creates a GDCHServiceAccountCredentials from a
  /// GDCHServiceAccountCredentialsInfo.
  static StatusOr<std::unique_ptr<Credentials>> CreateFromInfo(
      Info info, Options const& options, HttpClientFactory client_factory);

  /// Creates a GDCHServiceAccountCredentials from a JSON string.
  static StatusOr<std::unique_ptr<Credentials>> CreateFromJsonContents(
      std::string const& contents, std::string const& audience,
      Options const& options, HttpClientFactory client_factory);

  /// Creates a GDCHServiceAccountCredentials from a file at the specified path.
  static StatusOr<std::unique_ptr<Credentials>> CreateFromFilePath(
      std::string const& path, std::string const& audience,
      Options const& options, HttpClientFactory client_factory);

  /// Parses a refresh response JSON string to create an access token.
  static StatusOr<AccessToken> ParseRefreshResponse(
      rest_internal::RestResponse& response,
      std::chrono::system_clock::time_point now);

  /// Splits a GDCHServiceAccountCredentialsInfo into header and payload
  /// components and uses the current time to make a JWT assertion.
  ///
  /// @see https://tools.ietf.org/html/rfc7523
  static std::pair<std::string, std::string> AssertionComponentsFromInfo(
      Info const& info, std::chrono::system_clock::time_point now);

  /// Given a key and a JSON header and payload, creates a JWT assertion string.
  ///
  /// @see https://tools.ietf.org/html/rfc7519
  static StatusOr<std::string> MakeJWTAssertion(
      std::string const& header, std::string const& payload,
      std::string const& pem_contents);

  /// Uses a GDCHServiceAccountCredentialsInfo and the current time to construct
  /// a JWT assertion. The assertion combined with the grant_type and audience
  /// is used to create the refresh payload.
  static StatusOr<nlohmann::json> CreateRefreshPayload(
      Info const& info, std::chrono::system_clock::time_point now);

  StatusOr<AccessToken> GetToken(
      std::chrono::system_clock::time_point tp) override;

  std::string AccountEmail() const override {
    return info_.service_identity_name;
  }

  std::string KeyId() const override { return info_.private_key_id; }

  StatusOr<std::string> project_id() const override;
  StatusOr<std::string> project_id(Options const&) const override;

 private:
  GDCHServiceAccountCredentials(Info info, Options options,
                                HttpClientFactory client_factory);

  Info info_;
  Options options_;
  HttpClientFactory client_factory_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_GDCH_SERVICE_ACCOUNT_CREDENTIALS_H
