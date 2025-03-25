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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_MTLS_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_MTLS_CREDENTIALS_H

#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/mtls_credentials_config.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A `Credentials` type representing mTLS Google OAuth2.0 credentials.
 */
class MtlsCredentials : public oauth2_internal::Credentials {
 public:
  explicit MtlsCredentials(MtlsCredentialsConfig::Rest mtls_config_rest)
      : mtls_config_rest_(std::move(mtls_config_rest)) {}

  /**
   * While other Credentials subclasses return a string containing an
   * Authorization HTTP header from this method, this class always returns an
   * empty string as its value.
   */
  StatusOr<AccessToken> GetToken(
      std::chrono::system_clock::time_point tp) override;

  absl::optional<MtlsCredentialsConfig::Rest> MtlsConfig() const {
    return mtls_config_rest_;
  }

 private:
  MtlsCredentialsConfig::Rest mtls_config_rest_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_MTLS_CREDENTIALS_H
