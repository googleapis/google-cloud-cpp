// Copyright 2021 Google LLC
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

#include "google/cloud/internal/oauth2_minimal_iam_credentials_rest.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/json_parsing.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/internal/oauth2_universe_domain.h"
#include "google/cloud/internal/parse_rfc3339.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/log.h"
#include "google/cloud/options.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::internal::InvalidArgumentError;

MinimalIamCredentialsRestStub::MinimalIamCredentialsRestStub(
    std::shared_ptr<oauth2_internal::Credentials> credentials, Options options,
    HttpClientFactory client_factory)
    : credentials_(std::move(credentials)),
      options_(std::move(options)),
      client_factory_(std::move(client_factory)) {}

StatusOr<google::cloud::AccessToken>
MinimalIamCredentialsRestStub::GenerateAccessToken(
    GenerateAccessTokenRequest const& request) {
  auto auth_header = AuthorizationHeader(*credentials_);
  if (!auth_header) return std::move(auth_header).status();

  rest_internal::RestRequest rest_request;
  rest_request.AddHeader(auth_header.value());
  rest_request.AddHeader("Content-Type", "application/json");
  rest_request.SetPath(MakeRequestPath(request));
  nlohmann::json payload{
      {"delegates", request.delegates},
      {"scope", request.scopes},
      {"lifetime", std::to_string(request.lifetime.count()) + "s"},
  };

  auto client = client_factory_(options_);
  rest_internal::RestContext context;
  auto response = client->Post(context, rest_request, {payload.dump()});
  if (!response) return std::move(response).status();
  return ParseGenerateAccessTokenResponse(
      **response,
      internal::ErrorContext(
          {{"gcloud-cpp.root.class", "MinimalIamCredentialsRestStub"},
           {"gcloud-cpp.root.function", __func__},
           {"serviceAccount", request.service_account}}));
}

std::string MinimalIamCredentialsRestStub::MakeRequestPath(
    GenerateAccessTokenRequest const& request) {
  // TODO(#13422): Do not use hardcoded IAM endpoint. Use Universe Domain
  // to build endpoint name.
  return absl::StrCat(
      "https://iamcredentials.googleapis.com/v1/projects/-/serviceAccounts/",
      request.service_account, ":generateAccessToken");
}

MinimalIamCredentialsRestLogging::MinimalIamCredentialsRestLogging(
    std::shared_ptr<MinimalIamCredentialsRest> child)
    : child_(std::move(child)) {}

StatusOr<google::cloud::AccessToken>
MinimalIamCredentialsRestLogging::GenerateAccessToken(
    GenerateAccessTokenRequest const& request) {
  GCP_LOG(INFO) << __func__
                << "() << {service_account=" << request.service_account
                << ", lifetime=" << std::to_string(request.lifetime.count())
                << "s, scopes=[" << absl::StrJoin(request.scopes, ",")
                << "], delegates=[" << absl::StrJoin(request.delegates, ",")
                << "]}";
  auto response = child_->GenerateAccessToken(request);
  if (!response) {
    GCP_LOG(INFO) << __func__ << "() >> status={" << response.status() << "}";
    return response;
  }
  GCP_LOG(INFO) << __func__
                << "() >> response={access_token=[censored], expiration="
                << google::cloud::internal::FormatRfc3339(response->expiration)
                << "}";
  return response;
}

StatusOr<AccessToken> ParseGenerateAccessTokenResponse(
    rest_internal::RestResponse& response,
    google::cloud::internal::ErrorContext const& ec) {
  if (IsHttpError(response)) return AsStatus(std::move(response));
  auto response_payload =
      rest_internal::ReadAll(std::move(response).ExtractPayload());
  if (!response_payload) return std::move(response_payload).status();
  auto parsed = nlohmann::json::parse(*response_payload, nullptr, false);
  if (!parsed.is_object()) {
    return InvalidArgumentError("cannot parse response as a JSON object",
                                GCP_ERROR_INFO().WithContext(ec));
  }
  auto token = ValidateStringField(parsed, "accessToken",
                                   "GenerateAccessToken() response", ec);
  if (!token) return std::move(token).status();
  auto expire_time_field = ValidateStringField(
      parsed, "expireTime", "GenerateAccessToken() response", ec);
  if (!expire_time_field) return std::move(expire_time_field).status();
  auto expire_time = google::cloud::internal::ParseRfc3339(*expire_time_field);
  if (!expire_time) {
    return InvalidArgumentError(
        "invalid format for `expireTime` field in `GenerateAccessToken() "
        "response`",
        GCP_ERROR_INFO().WithContext(ec));
  }
  return google::cloud::AccessToken{*std::move(token), *expire_time};
}

std::shared_ptr<MinimalIamCredentialsRest> MakeMinimalIamCredentialsRestStub(
    std::shared_ptr<oauth2_internal::Credentials> credentials, Options options,
    HttpClientFactory client_factory) {
  auto enable_logging =
      options.get<TracingComponentsOption>().count("rpc") != 0 ||
      options.get<TracingComponentsOption>().count("raw-client") != 0;
  std::shared_ptr<MinimalIamCredentialsRest> stub =
      std::make_shared<MinimalIamCredentialsRestStub>(
          std::move(credentials), std::move(options),
          std::move(client_factory));
  if (enable_logging) {
    stub = std::make_shared<MinimalIamCredentialsRestLogging>(std::move(stub));
  }
  return stub;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
