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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_LOGGING_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_LOGGING_CREDENTIALS_H

#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Log credentials usage and refreshes.
 *
 * Debugging problems with authentication can be fairly difficult. This
 * decorator is automatically inserted (twice) if
 * `google::cloud::TracingComponentsOption` includes `auth`. The decorator is
 * inserted before and after the caching layer, to show whether a cached token
 * or a new token is being used.
 *
 * @note To prevent leaking authentication secrets, the tokens are not logged in
 * full.
 *
 * @see https://cloud.google.com/docs/authentication/ for an overview of
 * authenticating to Google Cloud Platform APIs.
 */
class LoggingCredentials : public Credentials {
 public:
  LoggingCredentials(std::string phase, TracingOptions tracing_options,
                     std::shared_ptr<Credentials> impl);
  ~LoggingCredentials() override;

  StatusOr<internal::AccessToken> GetToken(
      std::chrono::system_clock::time_point now) override;
  StatusOr<std::vector<std::uint8_t>> SignBlob(
      absl::optional<std::string> const& signing_service_account,
      std::string const& string_to_sign) const override;
  std::string AccountEmail() const override;
  std::string KeyId() const override;

 private:
  std::string phase_;
  TracingOptions tracing_options_;
  std::shared_ptr<Credentials> impl_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_LOGGING_CREDENTIALS_H
