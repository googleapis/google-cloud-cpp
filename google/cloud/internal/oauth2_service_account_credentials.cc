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
#include "google/cloud/internal/openssl_util.h"
#include "google/cloud/internal/rest_response.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

auto constexpr kP12PrivateKeyIdMarker = "--unknown--";

bool ServiceAccountUseOAuth(ServiceAccountCredentialsInfo const& info) {
  if (info.private_key_id == kP12PrivateKeyIdMarker) return true;
  auto disable_jwt = google::cloud::internal::GetEnv(
      "GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT");
  return disable_jwt.has_value();
}

}  // namespace

using ::google::cloud::internal::MakeJWTAssertionNoThrow;

StatusOr<ServiceAccountCredentialsInfo> ParseServiceAccountCredentials(
    std::string const& content, std::string const& source,
    std::string const& default_token_uri) {
  auto credentials = nlohmann::json::parse(content, nullptr, false);
  if (credentials.is_discarded()) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials,"
                  "parsing failed on data loaded from " +
                      source);
  }
  std::string const private_key_id_key = "private_key_id";
  std::string const private_key_key = "private_key";
  std::string const token_uri_key = "token_uri";
  std::string const client_email_key = "client_email";
  for (auto const& key : {private_key_key, client_email_key}) {
    if (credentials.count(key) == 0) {
      return Status(StatusCode::kInvalidArgument,
                    "Invalid ServiceAccountCredentials, the " +
                        std::string(key) +
                        " field is missing on data loaded from " + source);
    }
    if (credentials.value(key, "").empty()) {
      return Status(StatusCode::kInvalidArgument,
                    "Invalid ServiceAccountCredentials, the " +
                        std::string(key) +
                        " field is empty on data loaded from " + source);
    }
  }
  // The token_uri field may be missing, but may not be empty:
  if (credentials.count(token_uri_key) != 0 &&
      credentials.value(token_uri_key, "").empty()) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid ServiceAccountCredentials, the " +
                      std::string(token_uri_key) +
                      " field is empty on data loaded from " + source);
  }
  return ServiceAccountCredentialsInfo{
      credentials.value(client_email_key, ""),
      credentials.value(private_key_id_key, ""),
      credentials.value(private_key_key, ""),
      // Some credential formats (e.g. gcloud's ADC file) don't contain a
      // "token_uri" attribute in the JSON object.  In this case, we try using
      // the default value.
      credentials.value(token_uri_key, default_token_uri),
      /*scopes*/ {},
      /*subject*/ {}};
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

StatusOr<RefreshingCredentialsWrapper::TemporaryToken>
ParseServiceAccountRefreshResponse(rest_internal::RestResponse& response,
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

  // Response should have the attributes "access_token", "expires_in", and
  // "token_type".
  std::string header_value = access_token.value("token_type", "");
  header_value += ' ';
  header_value += access_token.value("access_token", "");
  auto expires_in = std::chrono::seconds(access_token.value("expires_in", 0));
  auto new_expiration = now + expires_in;
  return RefreshingCredentialsWrapper::TemporaryToken{
      std::make_pair("Authorization", header_value), new_expiration};
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
    std::unique_ptr<rest_internal::RestClient> rest_client,
    CurrentTimeFn current_time_fn)
    : info_(std::move(info)),
      current_time_fn_(std::move(current_time_fn)),
      rest_client_(std::move(rest_client)),
      options_(internal::MergeOptions(
          std::move(options),
          Options{}.set<ServiceAccountCredentialsTokenUriOption>(
              info_.token_uri))) {
  if (!rest_client_) {
    rest_client_ = rest_internal::MakeDefaultRestClient(
        options_.get<ServiceAccountCredentialsTokenUriOption>(), options_);
  }
}

StatusOr<std::pair<std::string, std::string>>
ServiceAccountCredentials::AuthorizationHeader() {
  std::unique_lock<std::mutex> lock(mu_);
  return refreshing_creds_.AuthorizationHeader([this] { return Refresh(); });
}

StatusOr<std::vector<std::uint8_t>> ServiceAccountCredentials::SignBlob(
    std::string const& signing_account, std::string const& blob) const {
  if (signing_account != info_.client_email) {
    return Status(
        StatusCode::kInvalidArgument,
        "The current_credentials cannot sign blobs for " + signing_account);
  }
  return internal::SignUsingSha256(blob, info_.private_key);
}

bool ServiceAccountCredentials::UseOAuth() {
  return ServiceAccountUseOAuth(info_);
}

StatusOr<RefreshingCredentialsWrapper::TemporaryToken>
ServiceAccountCredentials::Refresh() {
  if (UseOAuth()) return RefreshOAuth();
  return RefreshSelfSigned();
}

StatusOr<RefreshingCredentialsWrapper::TemporaryToken>
ServiceAccountCredentials::RefreshOAuth() const {
  rest_internal::RestRequest request;
  auto payload = CreateServiceAccountRefreshPayload(info_, current_time_fn_());
  StatusOr<std::unique_ptr<rest_internal::RestResponse>> response =
      rest_client_->Post(request, payload);
  if (!response.ok()) return std::move(response).status();
  std::unique_ptr<rest_internal::RestResponse> real_response =
      std::move(response.value());
  if (real_response->StatusCode() >= 300)
    return AsStatus(std::move(*real_response));
  return ParseServiceAccountRefreshResponse(*real_response, current_time_fn_());
}

StatusOr<RefreshingCredentialsWrapper::TemporaryToken>
ServiceAccountCredentials::RefreshSelfSigned() const {
  auto const tp = current_time_fn_();
  auto token = MakeSelfSignedJWT(info_, tp);
  if (!token) return std::move(token).status();
  return RefreshingCredentialsWrapper::TemporaryToken{
      {"Authorization", "Bearer " + *token},
      tp + GoogleOAuthAccessTokenLifetime()};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
