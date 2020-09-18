// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_CREDENTIALS_H

#include "google/cloud/storage/signed_url_options.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <chrono>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {
/**
 * Interface for OAuth 2.0 credentials used to access Google Cloud services.
 *
 * Instantiating a specific kind of `Credentials` should usually be done via the
 * convenience methods declared in google_credentials.h.
 *
 * @see https://cloud.google.com/docs/authentication/ for an overview of
 * authenticating to Google Cloud Platform APIs.
 */
class Credentials {
 public:
  virtual ~Credentials() = default;

  /**
   * Attempts to obtain a value for the Authorization HTTP header.
   *
   * If unable to obtain a value for the Authorization header, which could
   * happen for `Credentials` that need to be periodically refreshed, the
   * underlying `Status` will indicate failure details from the refresh HTTP
   * request. Otherwise, the returned value will contain the Authorization
   * header to be used in HTTP requests.
   */
  virtual StatusOr<std::string> AuthorizationHeader() = 0;

  /**
   * Try to sign @p string_to_sign using @p service_account.
   *
   * Some %Credentials types can locally sign a blob, most often just on behalf
   * of an specific service account. This function returns an error if the
   * credentials cannot sign the blob at all, or if the service account is a
   * mismatch.
   */
  virtual StatusOr<std::vector<std::uint8_t>> SignBlob(
      SigningAccount const& service_account,
      std::string const& string_to_sign) const;

  /// Return the account's email associated with these credentials, if any.
  virtual std::string AccountEmail() const { return std::string{}; }

  /// Return the account's key_id associated with these credentials, if any.
  virtual std::string KeyId() const { return std::string{}; }
};

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_CREDENTIALS_H
