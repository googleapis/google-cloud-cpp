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

#include "google/cloud/storage/oauth2/service_account_credentials.h"
#include "google/cloud/storage/internal/make_jwt_assertion.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/oauth2_service_account_credentials.h"
#include <nlohmann/json.hpp>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <array>
#include <cstdio>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace oauth2 {

auto constexpr kP12PrivateKeyIdMarker = "--unknown--";

StatusOr<ServiceAccountCredentialsInfo> ParseServiceAccountCredentials(
    std::string const& content, std::string const& source,
    std::string const& default_token_uri) {
  auto credentials = nlohmann::json::parse(content, nullptr, false);
  if (!credentials.is_object()) {
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

StatusOr<ServiceAccountCredentialsInfo> ParseServiceAccountP12File(
    std::string const& source, std::string const&) {
  auto info = oauth2_internal::ParseServiceAccountP12File(source);
  if (!info.ok()) return info.status();
  return ServiceAccountCredentialsInfo{info->client_email, info->private_key_id,
                                       info->private_key,  info->token_uri,
                                       info->scopes,       info->subject};
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

std::string CreateServiceAccountRefreshPayload(
    ServiceAccountCredentialsInfo const& info, std::string const& grant_type,
    std::chrono::system_clock::time_point now) {
  auto assertion_components = AssertionComponentsFromInfo(info, now);
  std::string payload = grant_type;
  payload += "&assertion=";
  payload += MakeJWTAssertion(assertion_components.first,
                              assertion_components.second, info.private_key);
  return payload;
}

StatusOr<RefreshingCredentialsWrapper::TemporaryToken>
ParseServiceAccountRefreshResponse(
    storage::internal::HttpResponse const& response,
    std::chrono::system_clock::time_point now) {
  auto access_token = nlohmann::json::parse(response.payload, nullptr, false);
  if (access_token.is_discarded() || access_token.count("access_token") == 0 or
      access_token.count("expires_in") == 0 or
      access_token.count("token_type") == 0) {
    auto payload =
        response.payload +
        "Could not find all required fields in response (access_token,"
        " expires_in, token_type) while trying to obtain an access token for"
        " service account credentials.";
    return AsStatus(storage::internal::HttpResponse{response.status_code,
                                                    payload, response.headers});
  }
  // Response should have the attributes "access_token", "expires_in", and
  // "token_type".
  std::string header =
      "Authorization: " + access_token.value("token_type", "") + " " +
      access_token.value("access_token", "");
  auto expires_in = std::chrono::seconds(access_token.value("expires_in", 0));
  auto new_expiration = now + expires_in;

  return RefreshingCredentialsWrapper::TemporaryToken{std::move(header),
                                                      new_expiration};
}

StatusOr<std::string> MakeSelfSignedJWT(
    ServiceAccountCredentialsInfo const& info,
    std::chrono::system_clock::time_point tp) {
  // This only runs about once an hour, the copies are ugly, but should be
  // harmless.
  oauth2_internal::ServiceAccountCredentialsInfo mapped;
  mapped.client_email = info.client_email;
  mapped.private_key_id = info.private_key_id;
  mapped.private_key = info.private_key;
  mapped.token_uri = info.token_uri;
  mapped.scopes = info.scopes;
  mapped.subject = info.subject;
  return ::google::cloud::oauth2_internal::MakeSelfSignedJWT(mapped, tp);
}

bool ServiceAccountUseOAuth(ServiceAccountCredentialsInfo const& info) {
  if (info.private_key_id == kP12PrivateKeyIdMarker) return true;
  auto disable_jwt = google::cloud::internal::GetEnv(
      "GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT");
  return disable_jwt.has_value();
}

}  // namespace oauth2
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
