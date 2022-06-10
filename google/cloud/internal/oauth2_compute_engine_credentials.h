// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_COMPUTE_ENGINE_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_COMPUTE_ENGINE_CREDENTIALS_H

#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/internal/oauth2_refreshing_credentials_wrapper.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include <chrono>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// A helper struct that contains service account metadata.
struct ServiceAccountMetadata {
  std::set<std::string> scopes;
  std::string email;
};

/// Parses a metadata server response JSON string into a ServiceAccountMetadata.
StatusOr<ServiceAccountMetadata> ParseMetadataServerResponse(
    rest_internal::RestResponse& response);

/**
 * Parses a metadata server response JSON string into a ServiceAccountMetadata.
 *
 * This function ignores all parsing errors, the data is purely informational,
 * it is better to just return nothing than to fail authentication because some
 * (most likely unused) data was not available or the service returned a
 * malformed response.
 */
ServiceAccountMetadata ParseMetadataServerResponse(std::string const& payload);

/// Parses a refresh response JSON string into an authorization header. The
/// header and the current time (for the expiration) form a TemporaryToken.
StatusOr<RefreshingCredentialsWrapper::TemporaryToken>
ParseComputeEngineRefreshResponse(rest_internal::RestResponse& response,
                                  std::chrono::system_clock::time_point now);

/**
 * Wrapper class for Google OAuth 2.0 GCE instance service account credentials.
 *
 * Takes a service account email address or alias (e.g. "default") and uses the
 * Google Compute Engine instance's metadata server to obtain service account
 * metadata and OAuth 2.0 access tokens as needed. Instances of this class
 * should usually be created via the convenience methods declared in
 * google_credentials.h.
 *
 * An HTTP Authorization header, with an access token as its value, can be
 * obtained by calling the AuthorizationHeader() method; if the current access
 * token is invalid or nearing expiration, this class will first obtain a new
 * access token before returning the Authorization header string.
 *
 * @see https://cloud.google.com/compute/docs/authentication#using for details
 * on how to get started with Compute Engine service account credentials.
 */
class ComputeEngineCredentials : public Credentials {
 public:
  using CurrentTimeFn =
      std::function<std::chrono::time_point<std::chrono::system_clock>()>;

  explicit ComputeEngineCredentials();

  /**
   * Creates an instance of ComputeEngineCredentials.
   *
   * @param rest_client a dependency injection point. It makes it possible to
   *     mock internal libcurl wrappers. This should generally not be overridden
   *     except for testing.
   * @param current_time_fn a dependency injection point to fetch the current
   *     time. This should generally not be overridden except for testing.
   */
  explicit ComputeEngineCredentials(
      std::string service_account_email, Options options = {},
      std::unique_ptr<rest_internal::RestClient> rest_client = nullptr,
      CurrentTimeFn current_time_fn = std::chrono::system_clock::now);

  /**
   * Returns a key value pair for an "Authorization" header.
   */
  StatusOr<std::pair<std::string, std::string>> AuthorizationHeader() override;

  /**
   * Returns the current Service Account email.
   */
  std::string AccountEmail() const override;

  /**
   * Returns the email or alias of this credential's service account.
   *
   * @note This class must query the Compute Engine instance's metadata server
   * to fetch service account metadata. Because of this, if an alias (e.g.
   * "default") was supplied in place of an actual email address when
   * initializing this credential, that alias is returned as this credential's
   * email address if the credential has not been refreshed yet.
   */
  std::string service_account_email() const;

  /**
   * Returns the set of scopes granted to this credential's service account.
   *
   * @note Because this class must query the Compute Engine instance's metadata
   * server to fetch service account metadata, this method will return an empty
   * set if the credential has not been refreshed yet.
   */
  std::set<std::string> scopes() const;

 private:
  /**
   * Sends an HTTP GET request to the GCE metadata server.
   *
   * @see https://cloud.google.com/compute/docs/storing-retrieving-metadata for
   * an overview of retrieving information from the GCE metadata server.
   */
  StatusOr<std::unique_ptr<rest_internal::RestResponse>>
  DoMetadataServerGetRequest(std::string const& path, bool recursive) const;

  /**
   * Fetches metadata for an instance's service account.
   *
   * @see
   * https://cloud.google.com/compute/docs/access/create-enable-service-accounts-for-instances
   * for more details.
   */
  Status RetrieveServiceAccountInfo() const;

  /**
   * Attempts to refresh the credentials.
   */
  StatusOr<RefreshingCredentialsWrapper::TemporaryToken> Refresh() const;

  mutable std::mutex mu_;
  CurrentTimeFn current_time_fn_;
  std::unique_ptr<rest_internal::RestClient> rest_client_;
  RefreshingCredentialsWrapper refreshing_creds_;
  mutable std::set<std::string> scopes_;
  mutable std::string service_account_email_;
  Options options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_COMPUTE_ENGINE_CREDENTIALS_H
