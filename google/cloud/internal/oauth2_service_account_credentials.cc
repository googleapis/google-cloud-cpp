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

#include "google/cloud/internal/oauth2_service_account_credentials.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/make_jwt_assertion.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/oauth2_google_credentials.h"
#include "google/cloud/internal/oauth2_universe_domain.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/internal/sign_using_sha256.h"
#include <nlohmann/json.hpp>
#include <functional>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::internal::MakeJWTAssertionNoThrow;

StatusOr<ServiceAccountCredentialsInfo> ParseServiceAccountCredentials(
    std::string const& content, std::string const& source,
    std::string const& default_token_uri) {
  auto credentials = nlohmann::json::parse(content, nullptr, false);
  if (credentials.is_discarded()) {
    return internal::InvalidArgumentError(
        absl::StrCat("Invalid ServiceAccountCredentials, parsing failed on ",
                     "data loaded from ", source));
  }

  using Validator =
      std::function<Status(absl::string_view name, nlohmann::json::iterator)>;
  using Store = std::function<void(ServiceAccountCredentialsInfo&,
                                   nlohmann::json::iterator const&)>;

  auto optional_field = [](absl::string_view, nlohmann::json::iterator const&) {
    return Status{};
  };
  auto non_empty_field = [&](absl::string_view name,
                             nlohmann::json::iterator const& l) {
    if (l == credentials.end()) return Status{};
    if (!l->get<std::string>().empty()) return Status{};
    return internal::InvalidArgumentError(
        absl::StrCat("Invalid ServiceAccountCredentials, the ", name,
                     " field is empty on data loaded from ", source));
  };
  auto required_field = [&](absl::string_view name,
                            nlohmann::json::iterator const& l) {
    if (l == credentials.end()) {
      return internal::InvalidArgumentError(
          absl::StrCat("Invalid ServiceAccountCredentials, the ", name,
                       " field is missing on data loaded from ", source));
    }
    return non_empty_field(name, l);
  };

  struct Field {
    std::string name;
    Validator validator;
    Store store;
  };
  std::vector<Field> fields{
      {"client_email", required_field,
       [](ServiceAccountCredentialsInfo& info,
          nlohmann::json::iterator const& l) {
         info.client_email = l->get<std::string>();
       }},
      {"private_key", required_field,
       [](ServiceAccountCredentialsInfo& info,
          nlohmann::json::iterator const& l) {
         info.private_key = l->get<std::string>();
       }},
      {"private_key_id", optional_field,
       [&](ServiceAccountCredentialsInfo& info,
           nlohmann::json::iterator const& l) {
         if (l == credentials.end()) return;
         info.private_key_id = l->get<std::string>();
       }},
      // Some credential formats (e.g. gcloud's ADC file) don't contain a
      // "token_uri" attribute in the JSON object.  In this case, we try using
      // the default value.
      {"token_uri", non_empty_field,
       [&](ServiceAccountCredentialsInfo& info,
           nlohmann::json::iterator const& l) {
         info.token_uri =
             l == credentials.end() ? default_token_uri : l->get<std::string>();
       }},
      {"universe_domain", non_empty_field,
       [&](ServiceAccountCredentialsInfo& info,
           nlohmann::json::iterator const& l) {
         info.universe_domain = l == credentials.end()
                                    ? GoogleDefaultUniverseDomain()
                                    : l->get<std::string>();
       }},
      {"project_id", non_empty_field,
       [&](ServiceAccountCredentialsInfo& info,
           nlohmann::json::iterator const& l) {
         if (l == credentials.end()) return;
         info.project_id = l->get<std::string>();
       }},
  };

  auto info = ServiceAccountCredentialsInfo{};
  info.enable_self_signed_jwt = true;
  for (auto& f : fields) {
    auto l = credentials.find(f.name);
    if (l != credentials.end() && !l->is_string()) {
      return internal::InvalidArgumentError(absl::StrCat(
          "Invalid ServiceAccountCredentials, the ", f.name,
          " field is present and is not a string, on data loaded from ",
          source));
    }
    auto status = f.validator(f.name, l);
    if (!status.ok()) return status;
    f.store(info, l);
  }
  return info;
}

std::pair<std::string, std::string> AssertionComponentsFromInfo(
    ServiceAccountCredentialsInfo const& info,
    std::chrono::system_clock::time_point now) {
  nlohmann::json assertion_header = {{"alg", "RS256"}, {"typ", "JWT"}};
  if (!info.private_key_id.empty()) {
    assertion_header["kid"] = info.private_key_id;
  }

  // Scopes must be specified in a space separated string:
  //    https://google.aip.dev/auth/4112
  auto scopes = [&info]() -> std::string {
    if (!info.scopes) return GoogleOAuthScopeCloudPlatform();
    return absl::StrJoin(*(info.scopes), " ");
  }();

  auto expiration = now + GoogleOAuthAccessTokenLifetime();
  // As much as possible, do the time arithmetic using the std::chrono types.
  // Convert to an integer only when we are dealing with timestamps since the
  // epoch. Note that we cannot use `time_t` directly because that might be a
  // floating point.
  auto const now_from_epoch =
      static_cast<std::intmax_t>(std::chrono::system_clock::to_time_t(now));
  auto const expiration_from_epoch = static_cast<std::intmax_t>(
      std::chrono::system_clock::to_time_t(expiration));
  nlohmann::json assertion_payload = {
      {"iss", info.client_email},
      {"scope", scopes},
      {"aud", info.token_uri},
      {"iat", now_from_epoch},
      // Resulting access token should expire after one hour.
      {"exp", expiration_from_epoch}};
  if (info.subject) {
    assertion_payload["sub"] = *(info.subject);
  }

  // Note: we don't move here as it would prevent copy elision.
  return std::make_pair(assertion_header.dump(), assertion_payload.dump());
}

std::string MakeJWTAssertion(std::string const& header,
                             std::string const& payload,
                             std::string const& pem_contents) {
  return internal::MakeJWTAssertionNoThrow(header, payload, pem_contents)
      .value();
}

std::vector<std::pair<std::string, std::string>>
CreateServiceAccountRefreshPayload(ServiceAccountCredentialsInfo const& info,
                                   std::chrono::system_clock::time_point now) {
  std::string header;
  std::string payload;
  std::tie(header, payload) = AssertionComponentsFromInfo(info, now);
  return {{"grant_type", "urn:ietf:params:oauth:grant-type:jwt-bearer"},
          {"assertion", MakeJWTAssertion(header, payload, info.private_key)}};
}

StatusOr<AccessToken> ParseServiceAccountRefreshResponse(
    rest_internal::RestResponse& response,
    std::chrono::system_clock::time_point now) {
  auto status_code = response.StatusCode();
  auto payload = rest_internal::ReadAll(std::move(response).ExtractPayload());
  if (!payload.ok()) return std::move(payload).status();
  auto access_token = nlohmann::json::parse(*payload, nullptr, false);
  if (access_token.is_discarded() || access_token.count("access_token") == 0 ||
      access_token.count("expires_in") == 0 ||
      access_token.count("token_type") == 0) {
    auto error_payload =
        *payload +
        "Could not find all required fields in response (access_token,"
        " expires_in, token_type) while trying to obtain an access token for"
        " service account credentials.";
    return AsStatus(status_code, error_payload);
  }

  auto expires_in = std::chrono::seconds(access_token.value("expires_in", 0));
  return AccessToken{access_token.value("access_token", ""), now + expires_in};
}

StatusOr<std::string> MakeSelfSignedJWT(
    ServiceAccountCredentialsInfo const& info,
    std::chrono::system_clock::time_point tp) {
  auto scope = [&info]() -> std::string {
    if (!info.scopes.has_value()) return GoogleOAuthScopeCloudPlatform();
    if (info.scopes->empty()) return GoogleOAuthScopeCloudPlatform();
    return absl::StrJoin(*info.scopes, " ");
  };

  auto const header = nlohmann::json{
      {"alg", "RS256"}, {"typ", "JWT"}, {"kid", info.private_key_id}};
  // As much as possible, do the time arithmetic using the std::chrono types.
  // Convert to an integer only when we are dealing with timestamps since the
  // epoch. Note that we cannot use `time_t` directly because that might be a
  // floating point.
  auto const expiration = tp + GoogleOAuthAccessTokenLifetime();
  auto const iat =
      static_cast<std::intmax_t>(std::chrono::system_clock::to_time_t(tp));
  auto const exp = static_cast<std::intmax_t>(
      std::chrono::system_clock::to_time_t(expiration));
  auto payload = nlohmann::json{
      {"iss", info.client_email},
      {"sub", info.client_email},
      {"iat", iat},
      {"exp", exp},
      {"scope", scope()},
  };

  return MakeJWTAssertionNoThrow(header.dump(), payload.dump(),
                                 info.private_key);
}

ServiceAccountCredentials::ServiceAccountCredentials(
    ServiceAccountCredentialsInfo info, Options options,
    HttpClientFactory client_factory)
    : info_(std::move(info)),
      options_(internal::MergeOptions(
          std::move(options),
          Options{}.set<ServiceAccountCredentialsTokenUriOption>(
              info_.token_uri))),
      client_factory_(std::move(client_factory)) {}

StatusOr<AccessToken> ServiceAccountCredentials::GetToken(
    std::chrono::system_clock::time_point tp) {
  if (UseOAuth()) return GetTokenOAuth(tp);
  return GetTokenSelfSigned(tp);
}

StatusOr<std::vector<std::uint8_t>> ServiceAccountCredentials::SignBlob(
    absl::optional<std::string> const& signing_account,
    std::string const& blob) const {
  if (signing_account.has_value() &&
      signing_account.value() != info_.client_email) {
    return Status(StatusCode::kInvalidArgument,
                  "The current_credentials cannot sign blobs for " +
                      signing_account.value());
  }
  return internal::SignUsingSha256(blob, info_.private_key);
}

StatusOr<std::string> ServiceAccountCredentials::universe_domain() const {
  if (!info_.universe_domain.has_value()) {
    return internal::NotFoundError(
        "universe_domain is not present in the credentials");
  }
  return *info_.universe_domain;
}

StatusOr<std::string> ServiceAccountCredentials::universe_domain(
    Options const&) const {
  // universe_domain is stored locally, so any retry options are unnecessary.
  return universe_domain();
}

StatusOr<std::string> ServiceAccountCredentials::project_id() const {
  if (!info_.project_id.has_value()) {
    return internal::NotFoundError(
        "project_id is not present in the credentials");
  }
  return *info_.project_id;
}

StatusOr<std::string> ServiceAccountCredentials::project_id(
    Options const&) const {
  // universe_domain is stored locally, so any retry options are unnecessary.
  return project_id();
}

bool ServiceAccountUseOAuth(ServiceAccountCredentialsInfo const& info) {
  // Custom universe domains are only supported with JWT, not OAuth tokens.
  if (info.universe_domain.has_value() &&
      info.universe_domain != GoogleDefaultUniverseDomain()) {
    return false;
  }
  if (info.private_key_id == P12PrivateKeyIdMarker() ||
      !info.enable_self_signed_jwt) {
    return true;
  }
  auto disable_jwt = google::cloud::internal::GetEnv(
      "GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT");
  return disable_jwt.has_value();
}

bool ServiceAccountCredentials::UseOAuth() {
  return ServiceAccountUseOAuth(info_);
}

StatusOr<AccessToken> ServiceAccountCredentials::GetTokenOAuth(
    std::chrono::system_clock::time_point tp) const {
  auto client = client_factory_(options_);
  rest_internal::RestRequest request;
  request.SetPath(options_.get<ServiceAccountCredentialsTokenUriOption>());
  auto payload = CreateServiceAccountRefreshPayload(info_, tp);
  rest_internal::RestContext context;
  auto response = client->Post(context, request, payload);
  if (!response) return std::move(response).status();
  if (IsHttpError(**response)) return AsStatus(std::move(**response));
  return ParseServiceAccountRefreshResponse(**response, tp);
}

StatusOr<AccessToken> ServiceAccountCredentials::GetTokenSelfSigned(
    std::chrono::system_clock::time_point tp) const {
  auto token = MakeSelfSignedJWT(info_, tp);
  if (!token) return std::move(token).status();
  return AccessToken{*token, tp + GoogleOAuthAccessTokenLifetime()};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
