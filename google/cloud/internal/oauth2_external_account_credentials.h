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

#include "google/cloud/internal/error_metadata.h"
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

struct ExternalAccountInfo {
  std::string audience;
  std::string subject_token_type;
  std::string token_url;
  ExternalAccountTokenSource token_source;
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
  ExternalAccountInfo info_;
  HttpClientFactory client_factory_;
  Options options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_EXTERNAL_ACCOUNT_CREDENTIALS_H
