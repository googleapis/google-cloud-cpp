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
  auto info = oauth2_internal::ParseServiceAccountCredentials(
      content, source, default_token_uri);
  if (!info.ok()) return info.status();
  return ServiceAccountCredentialsInfo{info->client_email, info->private_key_id,
                                       info->private_key,  info->token_uri,
                                       info->scopes,       info->subject};
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
  return oauth2_internal::AssertionComponentsFromInfo(
      internal::MapServiceAccountCredentialsInfo(info), now);
}

std::string MakeJWTAssertion(std::string const& header,
                             std::string const& payload,
                             std::string const& pem_contents) {
  return internal::MakeJWTAssertionNoThrow(header, payload, pem_contents)
      .value();
}

std::string CreateServiceAccountRefreshPayload(
    ServiceAccountCredentialsInfo const& info, std::string const&,
    std::chrono::system_clock::time_point now) {
  auto params = oauth2_internal::CreateServiceAccountRefreshPayload(
      internal::MapServiceAccountCredentialsInfo(info), now);
  return absl::StrJoin(params, "&", absl::PairFormatter("="));
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
  auto mapped = internal::MapServiceAccountCredentialsInfo(info);
  return ::google::cloud::oauth2_internal::MakeSelfSignedJWT(mapped, tp);
}

bool ServiceAccountUseOAuth(ServiceAccountCredentialsInfo const& info) {
  if (info.private_key_id == kP12PrivateKeyIdMarker) return true;
  // Self-signed JWTs do not work in GCS if they have scopes.
  if (info.scopes.has_value()) return true;
  auto disable_jwt = google::cloud::internal::GetEnv(
      "GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT");
  return disable_jwt.has_value();
}

ServiceAccountCredentials<storage::internal::CurlRequestBuilder,
                          std::chrono::system_clock>::
    ServiceAccountCredentials(ServiceAccountCredentialsInfo info,
                              ChannelOptions const& options)
    : impl_(absl::make_unique<oauth2_internal::ServiceAccountCredentials>(
          internal::MapServiceAccountCredentialsInfo(std::move(info)),
          Options{}.set<CARootsFilePathOption>(options.ssl_root_path()),
          [](Options const& o) {
            return rest_internal::MakeDefaultRestClient(std::string{}, o);
          })) {}

}  // namespace oauth2

namespace internal {

oauth2_internal::ServiceAccountCredentialsInfo MapServiceAccountCredentialsInfo(
    oauth2::ServiceAccountCredentialsInfo info) {
  // Storage has more stringent requirements w.r.t. self-signed JWTs
  // than most services. Any scope makes the self-signed JWTs unusable with
  // storage, but they remain usable with other services. We need to disable
  // self-signed JWTs in the implementation class as it is unaware of the
  // storage service limitations.
  auto enable_self_signed_jwt = !ServiceAccountUseOAuth(info);
  return {std::move(info.client_email), std::move(info.private_key_id),
          std::move(info.private_key),  std::move(info.token_uri),
          std::move(info.scopes),       std::move(info.subject),
          enable_self_signed_jwt};
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
