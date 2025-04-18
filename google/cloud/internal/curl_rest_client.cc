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
#include "google/cloud/internal/curl_handle_factory.h"
#include "google/cloud/internal/curl_impl.h"
#include "google/cloud/internal/curl_options.h"
#include "google/cloud/internal/curl_rest_response.h"
#include "google/cloud/internal/oauth2_google_credentials.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/tracing_rest_client.h"
#include "google/cloud/internal/unified_rest_credentials.h"
#include "absl/strings/match.h"
#include "absl/strings/strip.h"

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

std::size_t constexpr kDefaultPooledCurlHandleFactorySize = 10;

Status MakeRequestWithPayload(
    CurlImpl::HttpMethod http_method, RestContext& context,
    RestRequest const& request, CurlImpl& impl,
    std::vector<absl::Span<char const>> const& payload) {
  // If no Content-Type is specified for the payload, default to
  // application/x-www-form-urlencoded and encode the payload accordingly before
  // making the request.
  auto content_type = request.GetHeader("Content-Type");
  if (content_type.empty()) content_type = context.GetHeader("Content-Type");
  if (content_type.empty()) {
    std::string encoded_payload;
    impl.SetHeader(
        HttpHeader("content-type", "application/x-www-form-urlencoded"));
    std::string concatenated_payload;
    for (auto const& p : payload) {
      concatenated_payload += std::string(p.begin(), p.end());
    }
    encoded_payload = impl.MakeEscapedString(concatenated_payload);
    impl.SetHeader(
        HttpHeader("Content-Length", std::to_string(encoded_payload.size())));
    return impl.MakeRequest(http_method, context,
                            {{encoded_payload.data(), encoded_payload.size()}});
  }

  std::size_t content_length = 0;
  for (auto const& p : payload) {
    content_length += p.size();
  }

  impl.SetHeader(HttpHeader("Content-Length", std::to_string(content_length)));
  return impl.MakeRequest(http_method, context, payload);
}

std::string FormatHostHeaderValue(absl::string_view hostname) {
  if (!absl::ConsumePrefix(&hostname, "https://")) {
    absl::ConsumePrefix(&hostname, "http://");
  }
  return std::string(hostname.substr(0, hostname.find('/')));
}

std::unique_ptr<RestClient> MakeRestClient(
    std::string endpoint_address, std::shared_ptr<CurlHandleFactory> factory,
    Options options) {
  bool tracing_enabled = internal::TracingEnabled(options);
  std::unique_ptr<RestClient> client = std::make_unique<CurlRestClient>(
      std::move(endpoint_address), std::move(factory), std::move(options));
  if (tracing_enabled) client = MakeTracingRestClient(std::move(client));
  return client;
}

}  // namespace

HttpHeader CurlRestClient::HostHeader(Options const& options,
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
  if (!auth.empty()) return HttpHeader{"Host", auth};
  if (absl::StrContains(endpoint, "googleapis.com")) {
    return HttpHeader{"Host", FormatHostHeaderValue(endpoint)};
  }
  return HttpHeader{};
}

CurlRestClient::CurlRestClient(std::string endpoint_address,
                               std::shared_ptr<CurlHandleFactory> factory,
                               Options options)
    : endpoint_address_(std::move(endpoint_address)),
      handle_factory_(std::move(factory)),
      options_(std::move(options)) {
  if (options_.has<UnifiedCredentialsOption>()) {
    credentials_ = MapCredentials(*options_.get<UnifiedCredentialsOption>());
  }
}

StatusOr<std::unique_ptr<CurlImpl>> CurlRestClient::CreateCurlImpl(
    RestContext const& context, RestRequest const& request,
    Options const& options) {
  auto handle = CurlHandle::MakeFromPool(*handle_factory_);
  auto impl =
      std::make_unique<CurlImpl>(std::move(handle), handle_factory_, options);
  if (credentials_) {
    auto auth_header =
        credentials_->AuthenticationHeader(std::chrono::system_clock::now());
    if (!auth_header.ok()) return std::move(auth_header).status();
    impl->SetHeader(HttpHeader(auth_header->first, auth_header->second));
  }
  impl->SetHeader(HostHeader(options, endpoint_address_));
  impl->SetHeaders(context.headers());
  impl->SetHeaders(request.headers());

  RestRequest::HttpParameters additional_parameters;
  // The UserIp option has been deprecated in favor of quotaUser. Only add the
  // parameter if the option has been set.
  if (options.has<UserIpOption>()) {
    auto user_ip = options.get<UserIpOption>();
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
    RestContext& context, RestRequest const& request) {
  auto options = internal::MergeOptions(context.options(), options_);
  auto impl = CreateCurlImpl(context, request, options);
  if (!impl.ok()) return impl.status();
  auto response = (*impl)->MakeRequest(CurlImpl::HttpMethod::kDelete, context);
  if (!response.ok()) return response;
  return {std::unique_ptr<CurlRestResponse>(
      new CurlRestResponse(std::move(options), std::move(*impl)))};
}

StatusOr<std::unique_ptr<RestResponse>> CurlRestClient::Get(
    RestContext& context, RestRequest const& request) {
  auto options = internal::MergeOptions(context.options(), options_);
  auto impl = CreateCurlImpl(context, request, options);
  if (!impl.ok()) return impl.status();
  auto response = (*impl)->MakeRequest(CurlImpl::HttpMethod::kGet, context);
  if (!response.ok()) return response;
  return {std::unique_ptr<CurlRestResponse>(
      new CurlRestResponse(std::move(options), std::move(*impl)))};
}

StatusOr<std::unique_ptr<RestResponse>> CurlRestClient::Patch(
    RestContext& context, RestRequest const& request,
    std::vector<absl::Span<char const>> const& payload) {
  auto options = internal::MergeOptions(context.options(), options_);
  auto impl = CreateCurlImpl(context, request, options);
  if (!impl.ok()) return impl.status();
  Status response = MakeRequestWithPayload(CurlImpl::HttpMethod::kPatch,
                                           context, request, **impl, payload);
  if (!response.ok()) return response;
  return {std::unique_ptr<CurlRestResponse>(
      new CurlRestResponse(std::move(options), std::move(*impl)))};
}

StatusOr<std::unique_ptr<RestResponse>> CurlRestClient::Post(
    RestContext& context, RestRequest const& request,
    std::vector<absl::Span<char const>> const& payload) {
  auto options = internal::MergeOptions(context.options(), options_);
  auto impl = CreateCurlImpl(context, request, options);
  if (!impl.ok()) return impl.status();
  Status response = MakeRequestWithPayload(CurlImpl::HttpMethod::kPost, context,
                                           request, **impl, payload);
  if (!response.ok()) return response;
  return {std::unique_ptr<CurlRestResponse>(
      new CurlRestResponse(std::move(options), std::move(*impl)))};
}

StatusOr<std::unique_ptr<RestResponse>> CurlRestClient::Post(
    RestContext& context, RestRequest const& request,
    std::vector<std::pair<std::string, std::string>> const& form_data) {
  context.AddHeader("content-type", "application/x-www-form-urlencoded");
  auto options = internal::MergeOptions(context.options(), options_);
  auto impl = CreateCurlImpl(context, request, options);
  if (!impl.ok()) return impl.status();
  std::string form_payload = absl::StrJoin(
      form_data, "&",
      [&](std::string* out, std::pair<std::string, std::string> const& i) {
        out->append(
            absl::StrCat(i.first, "=", (*impl)->MakeEscapedString(i.second)));
      });
  Status response = MakeRequestWithPayload(CurlImpl::HttpMethod::kPost, context,
                                           request, **impl, {form_payload});
  if (!response.ok()) return response;
  return {std::unique_ptr<CurlRestResponse>(
      new CurlRestResponse(std::move(options), std::move(*impl)))};
}

StatusOr<std::unique_ptr<RestResponse>> CurlRestClient::Put(
    RestContext& context, RestRequest const& request,
    std::vector<absl::Span<char const>> const& payload) {
  auto options = internal::MergeOptions(context.options(), options_);
  auto impl = CreateCurlImpl(context, request, options);
  if (!impl.ok()) return impl.status();
  Status response = MakeRequestWithPayload(CurlImpl::HttpMethod::kPut, context,
                                           request, **impl, payload);
  if (!response.ok()) return response;
  return {std::unique_ptr<CurlRestResponse>(
      new CurlRestResponse(std::move(options), std::move(*impl)))};
}

std::unique_ptr<RestClient> MakeDefaultRestClient(std::string endpoint_address,
                                                  Options options) {
  auto factory = GetDefaultCurlHandleFactory(options);
  return MakeRestClient(std::move(endpoint_address), std::move(factory),
                        std::move(options));
}

std::unique_ptr<RestClient> MakePooledRestClient(std::string endpoint_address,
                                                 Options options) {
  std::size_t pool_size = kDefaultPooledCurlHandleFactorySize;
  if (options.has<ConnectionPoolSizeOption>()) {
    pool_size = options.get<ConnectionPoolSizeOption>();
  }
  auto pool = pool_size > 0 ? std::make_shared<PooledCurlHandleFactory>(
                                  pool_size, options)
                            : GetDefaultCurlHandleFactory(options);
  return MakeRestClient(std::move(endpoint_address), std::move(pool),
                        std::move(options));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
