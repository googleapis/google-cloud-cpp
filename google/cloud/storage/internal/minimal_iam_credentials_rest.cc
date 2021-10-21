// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/minimal_iam_credentials_rest.h"
#include "google/cloud/storage/internal/curl_handle_factory.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/parse_rfc3339.h"
#include "google/cloud/log.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

namespace {

class MinimalIamCredentialsRestImpl : public MinimalIamCredentialsRest {
 public:
  explicit MinimalIamCredentialsRestImpl(
      std::shared_ptr<oauth2::Credentials> credentials, Options options)
      : endpoint_(NormalizeEndpoint(options.get<RestEndpointOption>())),
        credentials_(std::move(credentials)),
        handle_factory_(std::make_shared<DefaultCurlHandleFactory>(options)),
        x_goog_api_client_header_("x-goog-api-client: " + x_goog_api_client()),
        options_(std::move(options)) {}

  StatusOr<google::cloud::internal::AccessToken> GenerateAccessToken(
      GenerateAccessTokenRequest const& request) override {
    auto auth_header = credentials_->AuthorizationHeader();
    if (!auth_header) return std::move(auth_header).status();

    CurlRequestBuilder builder(MakeRequestUrl(request), handle_factory_);
    builder.SetMethod("POST")
        .ApplyClientOptions(options_)
        .AddHeader(auth_header.value())
        .AddHeader(x_goog_api_client_header_);
    builder.AddHeader("Content-Type: application/json");
    nlohmann::json payload{
        {"delegates", request.delegates},
        {"scope", request.scopes},
        {"lifetime", std::to_string(request.lifetime.count()) + "s"},
    };
    auto response = builder.BuildRequest().MakeRequest(payload.dump());
    if (!response) return std::move(response).status();
    if (response->status_code >= HttpStatusCode::kMinNotSuccess) {
      return AsStatus(*response);
    }
    auto parsed = nlohmann::json::parse(response->payload, nullptr);
    if (parsed.is_null() || parsed.count("accessToken") == 0 ||
        parsed.count("expireTime") == 0) {
      return Status(StatusCode::kUnknown,
                    "invalid response from service <" + parsed.dump() + ">");
    }
    return google::cloud::internal::AccessToken{
        parsed["accessToken"].get<std::string>(),
        google::cloud::internal::ParseRfc3339(
            parsed["expireTime"].get<std::string>())};
  }

 private:
  std::string MakeRequestUrl(GenerateAccessTokenRequest const& request) {
    return absl::StrCat(endpoint_, "projects/-/serviceAccounts/",
                        request.service_account, ":generateAccessToken");
  }
  static std::string NormalizeEndpoint(std::string endpoint) {
    if (endpoint.empty()) return endpoint;
    if (endpoint.back() != '/') endpoint.push_back('/');
    return endpoint;
  }

  std::string endpoint_;
  std::shared_ptr<oauth2::Credentials> credentials_;
  std::shared_ptr<CurlHandleFactory> handle_factory_;
  std::string x_goog_api_client_header_;
  Options options_;
};

class MinimalIamCredentialsRestLogging : public MinimalIamCredentialsRest {
 public:
  explicit MinimalIamCredentialsRestLogging(
      std::shared_ptr<MinimalIamCredentialsRest> child)
      : child_(std::move(child)) {}

  StatusOr<google::cloud::internal::AccessToken> GenerateAccessToken(
      GenerateAccessTokenRequest const& request) override {
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
                  << google::cloud::internal::FormatRfc3339(
                         response->expiration)
                  << "}";
    return response;
  }

 private:
  std::shared_ptr<MinimalIamCredentialsRest> child_;
};

}  // namespace

std::shared_ptr<MinimalIamCredentialsRest> MakeMinimalIamCredentialsRestStub(
    std::shared_ptr<oauth2::Credentials> credentials, Options options) {
  options = google::cloud::internal::MergeOptions(
      Options{}.set<RestEndpointOption>(
          "https://iamcredentials.googleapis.com/v1/"),
      std::move(options));
  auto enable_logging =
      options.get<TracingComponentsOption>().count("rpc") != 0 ||
      options.get<TracingComponentsOption>().count("raw-client") != 0;
  std::shared_ptr<MinimalIamCredentialsRest> stub =
      std::make_shared<MinimalIamCredentialsRestImpl>(std::move(credentials),
                                                      std::move(options));
  if (enable_logging) {
    stub = std::make_shared<MinimalIamCredentialsRestLogging>(std::move(stub));
  }
  return stub;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
