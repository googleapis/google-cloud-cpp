// Copyright 2019 Google LLC
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

#include "google/cloud/internal/oauth2_compute_engine_credentials.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/compute_engine_util.h"
#include "google/cloud/internal/curl_options.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/oauth2_credential_constants.h"
#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/internal/oauth2_universe_domain.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/internal/rest_retry_loop.h"
#include "google/cloud/internal/retry_policy_impl.h"
#include "google/cloud/universe_domain_options.h"
#include "absl/strings/str_split.h"
#include <nlohmann/json.hpp>
#include <array>
#include <set>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

auto constexpr kMetadataServerUniverseDomainPath =
    "computeMetadata/v1/universe/universe-domain";

StatusOr<std::unique_ptr<rest_internal::RestResponse>>
DoMetadataServerGetRequest(rest_internal::RestClient& client,
                           std::string const& path, bool recursive) {
  rest_internal::RestRequest request;
  request.SetPath(absl::StrCat(internal::GceMetadataScheme(), "://",
                               internal::GceMetadataHostname(), "/", path));
  request.AddHeader("metadata-flavor", "Google");
  if (recursive) request.AddQueryParameter("recursive", "true");
  rest_internal::RestContext context;
  return client.Get(context, request);
}

struct DefaultUniverseDomainRetryTraits {
  static bool IsPermanentFailure(google::cloud::Status const& status) {
    return status.code() != StatusCode::kOk &&
           status.code() != StatusCode::kUnavailable;
  }
};

auto constexpr kDefaultUniverseDomainRetryDuration = std::chrono::seconds(60);
auto constexpr kDefaultUniverseDomainBackoffScaling = 2.0;

class DefaultUniverseDomainRetryPolicy
    : public internal::UniverseDomainRetryPolicy {
 public:
  ~DefaultUniverseDomainRetryPolicy() override = default;

  template <typename DurationRep, typename DurationPeriod>
  explicit DefaultUniverseDomainRetryPolicy(
      std::chrono::duration<DurationRep, DurationPeriod> maximum_duration)
      : impl_(maximum_duration) {}
  DefaultUniverseDomainRetryPolicy(
      DefaultUniverseDomainRetryPolicy&& rhs) noexcept
      : DefaultUniverseDomainRetryPolicy(rhs.maximum_duration()) {}
  DefaultUniverseDomainRetryPolicy(
      DefaultUniverseDomainRetryPolicy const& rhs) noexcept
      : DefaultUniverseDomainRetryPolicy(rhs.maximum_duration()) {}

  bool OnFailure(Status const& status) override {
    return impl_.OnFailure(status);
  }
  bool IsExhausted() const override { return impl_.IsExhausted(); }
  bool IsPermanentFailure(Status const& status) const override {
    return impl_.IsPermanentFailure(status);
  }
  std::chrono::milliseconds maximum_duration() const {
    return impl_.maximum_duration();
  }
  std::unique_ptr<RetryPolicy> clone() const override {
    return std::make_unique<DefaultUniverseDomainRetryPolicy>(
        maximum_duration());
  }

 private:
  internal::LimitedTimeRetryPolicy<DefaultUniverseDomainRetryTraits> impl_;
};

Options UniverseDomainDefaultPolicyOptions(Options const& options) {
  Options default_policy_options;
  if (!options.has<internal::UniverseDomainRetryPolicyOption>()) {
    default_policy_options.set<internal::UniverseDomainRetryPolicyOption>(
        std::make_unique<DefaultUniverseDomainRetryPolicy>(
            kDefaultUniverseDomainRetryDuration));
  }
  if (!options.has<internal::UniverseDomainBackoffPolicyOption>()) {
    default_policy_options.set<internal::UniverseDomainBackoffPolicyOption>(
        ExponentialBackoffPolicy(
            std::chrono::seconds(0), std::chrono::seconds(1),
            std::chrono::minutes(5), kDefaultUniverseDomainBackoffScaling,
            kDefaultUniverseDomainBackoffScaling)
            .clone());
  }
  return default_policy_options;
}

}  // namespace

StatusOr<ServiceAccountMetadata> ParseMetadataServerResponse(
    rest_internal::RestResponse& response) {
  auto payload = rest_internal::ReadAll(std::move(response).ExtractPayload());
  if (!payload.ok()) return payload.status();
  return ParseMetadataServerResponse(*payload);
}

ServiceAccountMetadata ParseMetadataServerResponse(std::string const& payload) {
  auto body = nlohmann::json::parse(payload, nullptr, false);
  // Parse the body, ignoring invalid or missing values.
  auto scopes = [&]() -> std::set<std::string> {
    auto it = body.find("scopes");
    if (it == body.end()) return {};
    if (it->is_string()) {
      return absl::StrSplit(it->get<std::string>(), '\n',
                            absl::SkipWhitespace());
    }
    // If we cannot parse the `scopes` field as an array of strings, we return
    // an empty set.
    if (!it->is_array()) return {};
    std::set<std::string> result;
    for (auto const& i : *it) {
      if (!i.is_string()) return {};
      result.insert(i.get<std::string>());
    }
    return result;
  };
  auto email = [&]() -> std::string {
    auto it = body.find("email");
    if (it == body.end() || !it->is_string()) return {};
    return it->get<std::string>();
  };
  auto universe_domain = [&]() -> std::string {
    auto iter = body.find("universe_domain");
    if (iter == body.end()) {
      return GoogleDefaultUniverseDomain();
    }
    // If the universe_domain field exists, but is the incorrect type, we don't
    // return the GDU in order to avoid leaking any auth data.
    if (!iter->is_string()) return {};
    return *iter;
  };
  return ServiceAccountMetadata{scopes(), email(), universe_domain()};
}

StatusOr<AccessToken> ParseComputeEngineRefreshResponse(
    rest_internal::RestResponse& response,
    std::chrono::system_clock::time_point now) {
  // Response should have the attributes "access_token", "expires_in", and
  // "token_type".
  auto payload = rest_internal::ReadAll(std::move(response).ExtractPayload());
  if (!payload.ok()) return payload.status();
  auto access_token = nlohmann::json::parse(*payload, nullptr, false);
  if (access_token.is_discarded() || access_token.count("access_token") == 0 ||
      access_token.count("expires_in") == 0 ||
      access_token.count("token_type") == 0) {
    auto error_payload =
        *payload +
        "Could not find all required fields in response (access_token,"
        " expires_in, token_type) while trying to obtain an access token for"
        " compute engine credentials.";
    return InvalidArgumentError(error_payload, GCP_ERROR_INFO());
  }
  auto expires_in = std::chrono::seconds(access_token.value("expires_in", 0));
  auto new_expiration = now + expires_in;

  return AccessToken{access_token.value("access_token", ""), new_expiration};
}

ComputeEngineCredentials::ComputeEngineCredentials(
    Options options, HttpClientFactory client_factory)
    : ComputeEngineCredentials("default", std::move(options),
                               std::move(client_factory)) {}

ComputeEngineCredentials::ComputeEngineCredentials(
    std::string service_account_email, Options options,
    HttpClientFactory client_factory)
    : options_(std::move(options)),
      client_factory_(std::move(client_factory)),
      service_account_email_(std::move(service_account_email)) {}

StatusOr<AccessToken> ComputeEngineCredentials::GetToken(
    std::chrono::system_clock::time_point tp) {
  // Ignore failures fetching the account metadata. We can still get a token
  // using the initial `service_account_email_` value.
  auto email = RetrieveServiceAccountInfo();
  auto client = client_factory_(options_);
  auto response = DoMetadataServerGetRequest(
      *client,
      "computeMetadata/v1/instance/service-accounts/" + email + "/token",
      false);
  if (!response) return std::move(response).status();
  if (IsHttpError(**response)) return AsStatus(std::move(**response));
  return ParseComputeEngineRefreshResponse(**response, tp);
}

std::string ComputeEngineCredentials::AccountEmail() const {
  std::lock_guard<std::mutex> lock(service_account_mu_);
  // Force a refresh on the account info.
  RetrieveServiceAccountInfo(lock);
  return service_account_email_;
}

StatusOr<std::string> ComputeEngineCredentials::universe_domain() const {
  std::lock_guard<std::mutex> lock(universe_domain_mu_);
  return RetrieveUniverseDomain(lock, Options{});
}

StatusOr<std::string> ComputeEngineCredentials::universe_domain(
    google::cloud::Options const& options) const {
  std::lock_guard<std::mutex> lock(universe_domain_mu_);
  return RetrieveUniverseDomain(lock, options);
}

StatusOr<std::string> ComputeEngineCredentials::project_id() const {
  return project_id(Options{});
}

StatusOr<std::string> ComputeEngineCredentials::project_id(
    google::cloud::Options const& options) const {
  std::lock_guard<std::mutex> lk(project_id_mu_);
  return RetrieveProjectId(lk, options);
}

StatusOr<std::string> ComputeEngineCredentials::RetrieveUniverseDomain(
    std::lock_guard<std::mutex> const&, Options const& options) const {
  // Fetch the universe domain only once.
  if (universe_domain_.has_value()) return *universe_domain_;

  Options merged_options = internal::MergeOptions(
      options, internal::MergeOptions(
                   options_, UniverseDomainDefaultPolicyOptions(options_)));
  auto client = client_factory_(merged_options);
  rest_internal::RestRequest request;
  request.SetPath(absl::StrCat(internal::GceMetadataScheme(), "://",
                               internal::GceMetadataHostname(), "/",
                               kMetadataServerUniverseDomainPath));
  request.AddHeader("metadata-flavor", "Google");
  request.AddQueryParameter("recursive", "true");

  auto current_options = internal::SaveCurrentOptions();
  StatusOr<std::unique_ptr<rest_internal::RestResponse>> response =
      rest_internal::RestRetryLoop(
          merged_options.get<internal::UniverseDomainRetryPolicyOption>()
              ->clone(),
          merged_options.get<internal::UniverseDomainBackoffPolicyOption>()
              ->clone(),
          Idempotency::kIdempotent,
          [&client](rest_internal::RestContext& rest_context, Options const&,
                    rest_internal::RestRequest const& request) {
            return client->Get(rest_context, request);
          },
          *current_options, request, __func__);

  if (!response.ok()) return std::move(response).status();
  if (IsHttpError(**response)) {
    // MDS could be an older version that does not support universe_domain.
    if ((*response)->StatusCode() == rest_internal::HttpStatusCode::kNotFound) {
      return GoogleDefaultUniverseDomain();
    }
    return AsStatus(std::move(**response));
  }

  auto payload =
      rest_internal::ReadAll((std::move(**response)).ExtractPayload());
  if (!payload.ok()) return payload.status();
  universe_domain_ = *std::move(payload);
  return *universe_domain_;
}

std::string ComputeEngineCredentials::service_account_email() const {
  std::unique_lock<std::mutex> lock(service_account_mu_);
  return service_account_email_;
}

std::set<std::string> ComputeEngineCredentials::scopes() const {
  std::unique_lock<std::mutex> lock(service_account_mu_);
  return scopes_;
}

std::string ComputeEngineCredentials::RetrieveServiceAccountInfo() const {
  return RetrieveServiceAccountInfo(
      std::lock_guard<std::mutex>{service_account_mu_});
}

std::string ComputeEngineCredentials::RetrieveServiceAccountInfo(
    std::lock_guard<std::mutex> const&) const {
  // Fetch the metadata only once.
  if (service_account_retrieved_) return service_account_email_;

  auto client = client_factory_(options_);
  auto response = DoMetadataServerGetRequest(
      *client,
      "computeMetadata/v1/instance/service-accounts/" + service_account_email_ +
          "/",
      true);
  if (!response || IsHttpError(**response)) return service_account_email_;
  auto metadata = ParseMetadataServerResponse(**response);
  if (!metadata) return service_account_email_;
  service_account_email_ = std::move(metadata->email);
  scopes_ = std::move(metadata->scopes);
  service_account_retrieved_ = true;
  return service_account_email_;
}

StatusOr<std::string> ComputeEngineCredentials::RetrieveProjectId(
    std::lock_guard<std::mutex> const&, Options const& options) const {
  if (project_id_.has_value()) return *project_id_;

  auto client = client_factory_(internal::MergeOptions(options, options_));
  auto request =
      rest_internal::RestRequest{}
          .SetPath(absl::StrCat(internal::GceMetadataScheme(), "://",
                                internal::GceMetadataHostname(), "/",
                                "computeMetadata/v1/project/project-id"))
          .AddHeader("metadata-flavor", "Google");

  auto context = rest_internal::RestContext{};
  auto response = client->Get(context, request);
  if (!response) return std::move(response).status();
  if (IsHttpError(**response)) return AsStatus(std::move(**response));

  auto payload =
      rest_internal::ReadAll((std::move(**response)).ExtractPayload());
  if (!payload) return std::move(payload).status();
  project_id_ = *std::move(payload);
  return *project_id_;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
