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
#include "google/cloud/storage/internal/compute_engine_util.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/oauth2/credential_constants.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/status.h"
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <mutex>
#include <set>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {

/**
 * A C++ wrapper for Google's Compute Engine Service Account Credentials.
 *
 * Takes a service account email address or alias (e.g. "default") and uses
 * the Google Compute Engine VM instance's metadata server to obtain service
 * account metadata and OAuth2 access tokens.
 *
 * @see
 *   https://cloud.google.com/compute/docs/authentication#using
 *
 * @tparam HttpRequestBuilderType a dependency injection point. It makes it
 *     possible to mock the libcurl wrappers.
 */
template <typename HttpRequestBuilderType =
              storage::internal::CurlRequestBuilder>
class ComputeEngineCredentials : public Credentials {
 public:
  explicit ComputeEngineCredentials() : ComputeEngineCredentials("default") {}

  explicit ComputeEngineCredentials(std::string const& service_account_email)
      : expiration_time_(), service_account_email_(service_account_email) {}

  std::pair<google::cloud::storage::Status, std::string> AuthorizationHeader()
      override {
    using google::cloud::storage::Status;
    std::unique_lock<std::mutex> lock(mu_);
    if (IsValid()) {
      return std::make_pair(Status(), authorization_header_);
    }
    Status status = Refresh();
    return std::make_pair(
        status, status.ok() ? authorization_header_ : std::string(""));
  }

  /**
   * Returns the email or alias of this credential's service account.
   *
   * Note that this class must query the Compute Engine instance's metadata
   * server to fetch service account metadata. Because of this, if an alias
   * (e.g. "default") was supplied in place of an actual email address when
   * initializing this credential, that alias is returned as this credential's
   * email address if the credential has not been refreshed yet.
   */
  std::string service_account_email() { return service_account_email_; }

  /**
   * Returns the set of scopes granted to this credential's service account.
   *
   * Note that because this class must query the Compute Engine instance's
   * metadata server to fetch service account metadata, this method will return
   * an empty set if the credential has not been refreshed yet.
   */
  std::set<std::string> scopes() { return scopes_; }

 private:
  bool IsExpired() {
    auto now = std::chrono::system_clock::now();
    return now > (expiration_time_ - GoogleOAuthAccessTokenExpirationSlack());
  }

  bool IsValid() {
    return not authorization_header_.empty() and not IsExpired();
  }

  storage::internal::HttpResponse DoMetadataServerGetRequest(std::string path,
                                                             bool recursive) {
    std::string metadata_server_hostname =
        google::cloud::storage::internal::GceMetadataHostname();

    HttpRequestBuilderType request_builder(
        std::move("http://" + metadata_server_hostname + path),
        storage::internal::GetDefaultCurlHandleFactory());
    request_builder.AddHeader("metadata-flavor: Google");
    if (recursive) {
      request_builder.AddQueryParameter("recursive", "true");
    }
    return request_builder.BuildRequest().MakeRequest("");
  }

  storage::Status RetrieveServiceAccountInfo() {
    namespace nl = google::cloud::storage::internal::nl;
    auto response = DoMetadataServerGetRequest(
        "/computeMetadata/v1/instance/service-accounts/" +
            service_account_email_ + "/",
        true);
    if (response.status_code >= 300) {
      return storage::Status(response.status_code, std::move(response.payload));
    }

    nl::json response_body = nl::json::parse(response.payload, nullptr, false);
    // Note that the "scopes" attribute will always be present and contain a
    // JSON array. At minimum, for the request to succeed, the instance must
    // have been granted the scope that allows it to retrieve info from the
    // metadata server.
    if (response_body.is_discarded() or response_body.count("email") == 0U or
        response_body.count("scopes") == 0U) {
      return storage::Status(
          response.status_code, std::move(response.payload),
          "Could not find all required fields in response (email, scopes).");
    }

    std::string email = response_body.value("email", "");
    std::set<std::string> scopes_set = response_body["scopes"];

    // Do not update any state until all potential exceptions are raised.
    service_account_email_ = email;
    scopes_ = scopes_set;
    return storage::Status();
  }

  storage::Status Refresh() {
    namespace nl = storage::internal::nl;

    auto status = RetrieveServiceAccountInfo();
    if (!status.ok()) {
      return status;
    }

    auto response = DoMetadataServerGetRequest(
        "/computeMetadata/v1/instance/service-accounts/" +
            service_account_email_ + "/token",
        false);
    if (response.status_code >= 300) {
      return storage::Status(response.status_code, std::move(response.payload));
    }

    // Response should have the attributes "access_token", "expires_in", and
    // "token_type".
    nl::json access_token = nl::json::parse(response.payload, nullptr, false);
    if (access_token.is_discarded() or
        access_token.count("access_token") == 0U or
        access_token.count("expires_in") == 0U or
        access_token.count("token_type") == 0U) {
      return storage::Status(
          response.status_code, std::move(response.payload),
          "Could not find all required fields in response (access_token,"
          " expires_in, token_type).");
    }
    std::string header = "Authorization: ";
    header += access_token.value("token_type", "");
    header += ' ';
    header += access_token.value("access_token", "");
    auto expires_in =
        std::chrono::seconds(access_token.value("expires_in", int(0)));
    auto new_expiration = std::chrono::system_clock::now() + expires_in;

    // Do not update any state until all potential exceptions are raised.
    authorization_header_ = std::move(header);
    expiration_time_ = new_expiration;
    return storage::Status();
  }

  std::mutex mu_;
  std::condition_variable cv_;
  // Credential attributes
  std::string authorization_header_;
  std::chrono::system_clock::time_point expiration_time_;
  std::set<std::string> scopes_;
  std::string service_account_email_;
};

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_COMPUTE_ENGINE_CREDENTIALS_H_
