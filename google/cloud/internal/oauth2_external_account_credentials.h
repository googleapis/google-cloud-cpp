// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_EXTERNAL_ACCOUNT_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_EXTERNAL_ACCOUNT_CREDENTIALS_H

#include "google/cloud/internal/error_context.h"
#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/internal/oauth2_external_account_token_source.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <functional>
#include <memory>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The (optional) configuration for service account impersonation.
 *
 * External accounts may require a call to the IAM Credentials service to
 * convert the initial access token to a specific service account access token.
 * Yes, this means up to 3 tokens may be involved:
 * - First the subject token obtained from a file, URL, or external program.
 * - Then the access token exchanged from the subject token via Google's
 *   Secure Token Service (STS).
 * - And then the access token exchanged from the initial access token to a
 *   different service account via IAM credentials.
 *
 * The JSON representation of this configuration has the (optional)
 * `service_account_impersonation_url` field separate from the
 * `service_account_impersonation` field. The latter is also optional, and only
 * has an effect if `service_account_impersonation_url` is set. Furthermore,
 * `service_account_impersonation` is a JSON object with a single (optional)
 * `token_lifetime` field.  The `token_lifetime` field has a default value of
 * `3600` seconds.  All these levels of "optional" can be represented in C++,
 * but it is easier to just have a single struct wrapped in an optional.
 */
struct ExternalAccountImpersonationConfig {
  std::string url;
  std::chrono::seconds token_lifetime;
};

/**
 * An external account configuration.
 *
 * This structure represents the result of parsing an external account JSON
 * object configuration.
 */
struct ExternalAccountInfo {
  std::string audience;
  std::string subject_token_type;
  std::string token_url;
  ExternalAccountTokenSource token_source;
  absl::optional<ExternalAccountImpersonationConfig> impersonation_config;
};

/// Parse a JSON string with an external account configuration.
StatusOr<ExternalAccountInfo> ParseExternalAccountConfiguration(
    std::string const& configuration, internal::ErrorContext const& ec);

class ExternalAccountCredentials : public oauth2_internal::Credentials {
 public:
  ExternalAccountCredentials(ExternalAccountInfo info,
                             HttpClientFactory client_factory,
                             Options options = {});
  ~ExternalAccountCredentials() override = default;

  StatusOr<internal::AccessToken> GetToken(
      std::chrono::system_clock::time_point tp) override;

 private:
  StatusOr<internal::AccessToken> GetTokenImpersonation(
      std::string const& access_token, internal::ErrorContext const& ec);

  ExternalAccountInfo info_;
  HttpClientFactory client_factory_;
  Options options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_EXTERNAL_ACCOUNT_CREDENTIALS_H
