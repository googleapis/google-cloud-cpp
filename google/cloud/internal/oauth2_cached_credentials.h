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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_CACHED_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_CACHED_CREDENTIALS_H

#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/version.h"
#include <mutex>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Cache the access token returned by another `Credentials` object.
 *
 * Obtaining an access token can be expensive. It may involve one or more HTTP
 * requests.  Access tokens are time bound, but typically last about 60 minutes.
 * Caching their value until they are about to expire minimizes overhead.
 *
 * Even for tokens that do not require a HTTP request, caching their value may
 * save CPU resources, as creating tokens typically involve some kind of
 * cryptographic signature.
 *
 * @see https://cloud.google.com/docs/authentication/ for an overview of
 * authenticating to Google Cloud Platform APIs.
 */
class CachedCredentials : public Credentials {
 public:
  explicit CachedCredentials(std::shared_ptr<Credentials> impl);
  ~CachedCredentials() override;

  StatusOr<internal::AccessToken> GetToken(
      std::chrono::system_clock::time_point now) override;
  StatusOr<std::pair<std::string, std::string>> AuthorizationHeader() override;
  StatusOr<std::vector<std::uint8_t>> SignBlob(
      absl::optional<std::string> const& signing_service_account,
      std::string const& string_to_sign) const override;
  std::string AccountEmail() const override;
  std::string KeyId() const override;

 private:
  std::shared_ptr<Credentials> impl_;
  std::mutex mu_;
  internal::AccessToken token_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_CACHED_CREDENTIALS_H
