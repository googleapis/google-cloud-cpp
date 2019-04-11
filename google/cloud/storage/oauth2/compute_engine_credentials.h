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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_COMPUTE_ENGINE_CREDENTIALS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_COMPUTE_ENGINE_CREDENTIALS_H_

#include "google/cloud/internal/getenv.h"
#include "google/cloud/status.h"
#include "google/cloud/storage/internal/compute_engine_util.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/oauth2/credential_constants.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/oauth2/refreshing_credentials_wrapper.h"
#include "google/cloud/storage/version.h"
#include <ctime>
#include <mutex>
#include <set>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {

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
 * token is invalid or nearing expiration, this will class will first obtain a
 * new access token before returning the Authorization header string.
 *
 * @see https://cloud.google.com/compute/docs/authentication#using for details
 * on how to get started with Compute Engine service account credentials.
 *
 * @tparam HttpRequestBuilderType a dependency injection point. It makes it
 *     possible to mock internal libcurl wrappers. This should generally not
 *     be overridden except for testing.
 */
template <typename HttpRequestBuilderType =
              storage::internal::CurlRequestBuilder>
class ComputeEngineCredentials : public Credentials {
 public:
  explicit ComputeEngineCredentials() : ComputeEngineCredentials("default") {}

  explicit ComputeEngineCredentials(std::string const& service_account_email)
      : service_account_email_(service_account_email) {}

  StatusOr<std::string> AuthorizationHeader() override {
    std::unique_lock<std::mutex> lock(mu_);
    return refreshing_creds_.AuthorizationHeader([this] { return Refresh(); });
  }

  /**
   * Returns the email or alias of this credential's service account.
   *
   * @note This class must query the Compute Engine instance's metadata server
   * to fetch service account metadata. Because of this, if an alias (e.g.
   * "default") was supplied in place of an actual email address when
   * initializing this credential, that alias is returned as this credential's
   * email address if the credential has not been refreshed yet.
   */
  std::string service_account_email() {
    std::unique_lock<std::mutex> lock(mu_);
    return service_account_email_;
  }

  /**
   * Returns the set of scopes granted to this credential's service account.
   *
   * @note Because this class must query the Compute Engine instance's metadata
   * server to fetch service account metadata, this method will return an empty
   * set if the credential has not been refreshed yet.
   */
  std::set<std::string> scopes() {
    std::unique_lock<std::mutex> lock(mu_);
    return scopes_;
  }

 private:
  /**
   * Sends an HTTP GET request to the GCE metadata server.
   *
   * @see https://cloud.google.com/compute/docs/storing-retrieving-metadata for
   * an overview of retrieving information from the GCE metadata server.
   */
  StatusOr<storage::internal::HttpResponse> DoMetadataServerGetRequest(
      std::string path, bool recursive) {
    // Allows mocking the metadata server hostname for testing.
    std::string metadata_server_hostname =
        google::cloud::storage::internal::GceMetadataHostname();

    HttpRequestBuilderType request_builder(
        std::move("http://" + metadata_server_hostname + path),
        storage::internal::GetDefaultCurlHandleFactory());
    request_builder.AddHeader("metadata-flavor: Google");
    if (recursive) {
      request_builder.AddQueryParameter("recursive", "true");
    }
    return request_builder.BuildRequest().MakeRequest(std::string{});
  }

  /**
   * Fetches metadata for an instance's service account.
   *
   * @see
   * https://cloud.google.com/compute/docs/access/create-enable-service-accounts-for-instances
   * for more details.
   */
  Status RetrieveServiceAccountInfo() {
    namespace nl = google::cloud::storage::internal::nl;
    auto response = DoMetadataServerGetRequest(
        "/computeMetadata/v1/instance/service-accounts/" +
            service_account_email_ + "/",
        true);
    if (!response) {
      return std::move(response).status();
    }
    if (response->status_code >= 300) {
      return AsStatus(*response);
    }

    nl::json response_body = nl::json::parse(response->payload, nullptr, false);
    // Note that the "scopes" attribute will always be present and contain a
    // JSON array. At minimum, for the request to succeed, the instance must
    // have been granted the scope that allows it to retrieve info from the
    // metadata server.
    if (response_body.is_discarded() or response_body.count("email") == 0U or
        response_body.count("scopes") == 0U) {
      response->payload +=
          "Could not find all required fields in response (email, scopes).";
      return AsStatus(*response);
    }

    std::string email = response_body.value("email", "");
    std::set<std::string> scopes_set = response_body["scopes"];

    // Do not update any state until all potential exceptions are raised.
    service_account_email_ = email;
    scopes_ = scopes_set;
    return Status();
  }

  Status Refresh() {
    namespace nl = storage::internal::nl;

    auto status = RetrieveServiceAccountInfo();
    if (!status.ok()) {
      return status;
    }

    auto response = DoMetadataServerGetRequest(
        "/computeMetadata/v1/instance/service-accounts/" +
            service_account_email_ + "/token",
        false);
    if (!response) {
      return std::move(response).status();
    }
    if (response->status_code >= 300) {
      return AsStatus(*response);
    }

    // Response should have the attributes "access_token", "expires_in", and
    // "token_type".
    nl::json access_token = nl::json::parse(response->payload, nullptr, false);
    if (access_token.is_discarded() ||
        access_token.count("access_token") == 0U or
        access_token.count("expires_in") == 0U or
        access_token.count("token_type") == 0U) {
      response->payload +=
          "Could not find all required fields in response (access_token,"
          " expires_in, token_type).";
      return AsStatus(*response);
    }
    std::string header = "Authorization: ";
    header += access_token.value("token_type", "");
    header += ' ';
    header += access_token.value("access_token", "");
    auto expires_in =
        std::chrono::seconds(access_token.value("expires_in", int(0)));
    auto new_expiration = std::chrono::system_clock::now() + expires_in;

    // Do not update any state until all potential exceptions are raised.
    refreshing_creds_.authorization_header = std::move(header);
    refreshing_creds_.expiration_time = new_expiration;
    return Status();
  }

  mutable std::mutex mu_;
  RefreshingCredentialsWrapper refreshing_creds_;
  std::set<std::string> scopes_;
  std::string service_account_email_;
};

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_COMPUTE_ENGINE_CREDENTIALS_H_
