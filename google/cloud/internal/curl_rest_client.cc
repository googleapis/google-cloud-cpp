// Copyright 2022 Google LLC
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

#include "google/cloud/internal/curl_rest_client.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/curl_handle_factory.h"
#include "google/cloud/internal/curl_impl.h"
#include "google/cloud/internal/curl_options.h"
#include "google/cloud/internal/curl_rest_response.h"
#include "google/cloud/internal/oauth2_google_credentials.h"
#include "google/cloud/internal/rest_options.h"
#include "google/cloud/internal/unified_rest_credentials.h"
#include "google/cloud/log.h"
#include "absl/strings/match.h"
#include "absl/strings/strip.h"

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

std::size_t constexpr kDefaultPooledCurlHandleFactorySize = 10;

Status MakeRequestWithPayload(
    CurlImpl::HttpMethod http_method, RestRequest const& request,
    CurlImpl& impl, std::vector<absl::Span<char const>> const& payload) {
  // If no Content-Type is specified for the payload, default to
  // application/x-www-form-urlencoded and encode the payload accordingly before
  // making the request.
  auto content_type = request.GetHeader("Content-Type");
  if (content_type.empty()) {
    std::string encoded_payload;
    impl.SetHeader("content-type: application/x-www-form-urlencoded");
    std::string concatenated_payload;
    for (auto const& p : payload) {
      concatenated_payload += std::string(p.begin(), p.end());
    }
    encoded_payload = impl.MakeEscapedString(concatenated_payload);
    if (!encoded_payload.empty()) {
      impl.SetHeader(absl::StrCat("content-length: ", encoded_payload.size()));
    }
    return impl.MakeRequest(http_method,
                            {{encoded_payload.data(), encoded_payload.size()}});
  }

  std::size_t content_length = 0;
  for (auto const& p : payload) {
    content_length += p.size();
  }
  if (content_length > 0) {
    impl.SetHeader(absl::StrCat("content-length: ", content_length));
  }
  return impl.MakeRequest(http_method, payload);
}

std::string FormatHostHeaderValue(absl::string_view hostname) {
  if (!absl::ConsumePrefix(&hostname, "https://")) {
    absl::ConsumePrefix(&hostname, "http://");
  }
  return std::string(hostname.substr(0, hostname.find('/')));
}

}  // namespace

std::string CurlRestClient::HostHeader(Options const& options,
                                       std::string const& endpoint) {
  // If this function returns an empty string libcurl will fill out the `Host: `
  // header based on the URL. In most cases this is the correct value. The main
  // exception are applications using `VPC-SC`:
  //     https://cloud.google.com/vpc/docs/configure-private-google-access
  // In those cases the application would target a URL like
  // `https://restricted.googleapis.com`, or `https://private.googleapis.com`,
  // or their own proxy, and need to provide the target's service host via the
  // AuthorityOption.
  auto const& auth = options.get<AuthorityOption>();
  if (!auth.empty()) return absl::StrCat("Host: ", auth);
  if (absl::StrContains(endpoint, "googleapis.com")) {
    return absl::StrCat("Host: ", FormatHostHeaderValue(endpoint));
  }
  return {};
}

CurlRestClient::CurlRestClient(std::string endpoint_address,
                               std::shared_ptr<CurlHandleFactory> factory,
                               Options options)
    : endpoint_address_(std::move(endpoint_address)),
      handle_factory_(std::move(factory)),
      x_goog_api_client_header_("x-goog-api-client: " +
                                google::cloud::internal::ApiClientHeader()),
      options_(std::move(options)) {}

StatusOr<std::unique_ptr<CurlImpl>> CurlRestClient::CreateCurlImpl(
    RestRequest const& request) {
  auto handle = GetCurlHandle(handle_factory_);
  auto impl =
      absl::make_unique<CurlImpl>(std::move(handle), handle_factory_, options_);
  if (options_.has<UnifiedCredentialsOption>()) {
    auto credentials = MapCredentials(options_.get<UnifiedCredentialsOption>());
    auto auth_header = credentials->AuthorizationHeader();
    if (!auth_header.ok()) return std::move(auth_header).status();
    impl->SetHeader(auth_header.value());
  }
  impl->SetHeader(HostHeader(options_, endpoint_address_));
  impl->SetHeader(x_goog_api_client_header_);
  impl->SetHeaders(request);
  RestRequest::HttpParameters additional_parameters;
  // The UserIp option has been deprecated in favor of quotaUser. Only add the
  // parameter if the option has been set.
  if (options_.has<UserIpOption>()) {
    auto user_ip = options_.get<UserIpOption>();
    if (user_ip.empty()) user_ip = impl->LastClientIpAddress();
    if (!user_ip.empty()) additional_parameters.emplace_back("userIp", user_ip);
  }
  impl->SetUrl(endpoint_address_, request, additional_parameters);
  return impl;
}

// While the similar implementations of all these HTTP verb methods suggest
// refactoring is possible, there are some constraints that preclude it:
// 1 Pimpl: only certain types from curl_impl.h can be forward declared for use
//          in CurlRestClient member function declarations.
// 2 Access: CurlRestResponse constructor is private, CurlRestClient is a friend
//           CreateCurlImpl relies heavily on member variables.
StatusOr<std::unique_ptr<RestResponse>> CurlRestClient::Delete(
    RestRequest const& request) {
  auto impl = CreateCurlImpl(request);
  if (!impl.ok()) return impl.status();
  auto response = (*impl)->MakeRequest(CurlImpl::HttpMethod::kDelete);
  if (!response.ok()) return response;
  return {std::unique_ptr<CurlRestResponse>(
      new CurlRestResponse(options_, std::move(*impl)))};
}

StatusOr<std::unique_ptr<RestResponse>> CurlRestClient::Get(
    RestRequest const& request) {
  auto impl = CreateCurlImpl(request);
  if (!impl.ok()) return impl.status();
  auto response = (*impl)->MakeRequest(CurlImpl::HttpMethod::kGet);
  if (!response.ok()) return response;
  return {std::unique_ptr<CurlRestResponse>(
      new CurlRestResponse(options_, std::move(*impl)))};
}

StatusOr<std::unique_ptr<RestResponse>> CurlRestClient::Patch(
    RestRequest const& request,
    std::vector<absl::Span<char const>> const& payload) {
  auto impl = CreateCurlImpl(request);
  if (!impl.ok()) return impl.status();
  Status response = MakeRequestWithPayload(CurlImpl::HttpMethod::kPatch,
                                           request, **impl, payload);
  if (!response.ok()) return response;
  return {std::unique_ptr<CurlRestResponse>(
      new CurlRestResponse(options_, std::move(*impl)))};
}

StatusOr<std::unique_ptr<RestResponse>> CurlRestClient::Post(
    RestRequest const& request,
    std::vector<absl::Span<char const>> const& payload) {
  auto impl = CreateCurlImpl(request);
  if (!impl.ok()) return impl.status();
  Status response = MakeRequestWithPayload(CurlImpl::HttpMethod::kPost, request,
                                           **impl, payload);
  if (!response.ok()) return response;
  return {std::unique_ptr<CurlRestResponse>(
      new CurlRestResponse(options_, std::move(*impl)))};
}

StatusOr<std::unique_ptr<RestResponse>> CurlRestClient::Post(
    RestRequest request,
    std::vector<std::pair<std::string, std::string>> const& form_data) {
  auto impl = CreateCurlImpl(request);
  if (!impl.ok()) return impl.status();
  std::string form_payload = absl::StrJoin(
      form_data, "&",
      [&](std::string* out, std::pair<std::string, std::string> const& i) {
        out->append(
            absl::StrCat(i.first, "=", (*impl)->MakeEscapedString(i.second)));
      });
  request.AddHeader("content-type", "application/x-www-form-urlencoded");
  Status response = MakeRequestWithPayload(CurlImpl::HttpMethod::kPost, request,
                                           **impl, {form_payload});
  if (!response.ok()) return response;
  return {std::unique_ptr<CurlRestResponse>(
      new CurlRestResponse(options_, std::move(*impl)))};
}

StatusOr<std::unique_ptr<RestResponse>> CurlRestClient::Put(
    RestRequest const& request,
    std::vector<absl::Span<char const>> const& payload) {
  auto impl = CreateCurlImpl(request);
  if (!impl.ok()) return impl.status();
  Status response = MakeRequestWithPayload(CurlImpl::HttpMethod::kPut, request,
                                           **impl, payload);
  if (!response.ok()) return response;
  return {std::unique_ptr<CurlRestResponse>(
      new CurlRestResponse(options_, std::move(*impl)))};
}

std::unique_ptr<RestClient> MakeDefaultRestClient(std::string endpoint_address,
                                                  Options options) {
  auto factory = GetDefaultCurlHandleFactory(options);
  return std::unique_ptr<RestClient>(new CurlRestClient(
      std::move(endpoint_address), std::move(factory), std::move(options)));
}

std::unique_ptr<RestClient> MakePooledRestClient(std::string endpoint_address,
                                                 Options options) {
  std::size_t pool_size = kDefaultPooledCurlHandleFactorySize;
  if (options.has<ConnectionPoolSizeOption>() &&
      options.get<ConnectionPoolSizeOption>() > 0) {
    pool_size = options.get<ConnectionPoolSizeOption>();
  }
  auto factory = std::make_shared<PooledCurlHandleFactory>(pool_size, options);
  return std::unique_ptr<RestClient>(new CurlRestClient(
      std::move(endpoint_address), std::move(factory), std::move(options)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
