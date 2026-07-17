// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/curl_options.h"  // for HttpVersionOption
#include "google/cloud/internal/rest_background_threads_impl.h"
#include "google/cloud/internal/rest_client.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "google/showcase/v1beta1/echo_client.h"
#include "google/showcase/v1beta1/internal/echo_option_defaults.h"
#include "google/showcase/v1beta1/internal/echo_rest_connection_impl.h"
#include "google/showcase/v1beta1/internal/echo_rest_metadata_decorator.h"
#include "google/showcase/v1beta1/internal/echo_rest_stub.h"
#include <gtest/gtest.h>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace v1beta1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::rest_internal::RestClient;
using ::google::cloud::rest_internal::RestContext;
using ::google::cloud::rest_internal::RestRequest;
using ::google::cloud::rest_internal::RestResponse;

class HeaderInterceptingRestClient : public RestClient {
 public:
  HeaderInterceptingRestClient(
      std::shared_ptr<RestClient> delegate,
      std::function<void(std::multimap<std::string, std::string> const&)>
          header_callback)
      : delegate_(std::move(delegate)),
        header_callback_(std::move(header_callback)) {}

  StatusOr<std::unique_ptr<RestResponse>> Delete(
      RestContext& context, RestRequest const& request) override {
    auto response = delegate_->Delete(context, request);
    if (response) header_callback_((*response)->Headers());
    return response;
  }

  StatusOr<std::unique_ptr<RestResponse>> Get(
      RestContext& context, RestRequest const& request) override {
    auto response = delegate_->Get(context, request);
    if (response) header_callback_((*response)->Headers());
    return response;
  }

  StatusOr<std::unique_ptr<RestResponse>> Patch(
      RestContext& context, RestRequest const& request,
      std::vector<absl::Span<char const>> const& payload) override {
    auto response = delegate_->Patch(context, request, payload);
    if (response) header_callback_((*response)->Headers());
    return response;
  }

  StatusOr<std::unique_ptr<RestResponse>> Post(
      RestContext& context, RestRequest const& request,
      std::vector<absl::Span<char const>> const& payload) override {
    auto response = delegate_->Post(context, request, payload);
    if (response) header_callback_((*response)->Headers());
    return response;
  }

  StatusOr<std::unique_ptr<RestResponse>> Post(
      RestContext& context, RestRequest const& request,
      std::vector<std::pair<std::string, std::string>> const& form_data)
      override {
    auto response = delegate_->Post(context, request, form_data);
    if (response) header_callback_((*response)->Headers());
    return response;
  }

  StatusOr<std::unique_ptr<RestResponse>> Put(
      RestContext& context, RestRequest const& request,
      std::vector<absl::Span<char const>> const& payload) override {
    auto response = delegate_->Put(context, request, payload);
    if (response) header_callback_((*response)->Headers());
    return response;
  }

 private:
  std::shared_ptr<RestClient> delegate_;
  std::function<void(std::multimap<std::string, std::string> const&)>
      header_callback_;
};

TEST(EchoRestIntegrationTest, EchoSuccessRestWithPqcVerification) {
  std::string ca_path;
  if (auto* ca_env = std::getenv("SHOWCASE_CA_CERT")) {
    ca_path = ca_env;
  } else {
    auto* test_srcdir = std::getenv("TEST_SRCDIR");
    ASSERT_NE(test_srcdir, nullptr);
    ca_path = std::string(test_srcdir) + "/_main/ci/showcase/showcase.pem";
  }

  std::ifstream ca_file(ca_path);
  ASSERT_TRUE(ca_file.good()) << "Failed to open CA file at " << ca_path;

  std::string port = "7469";
  if (auto* port_env = std::getenv("SHOWCASE_PORT")) {
    port = port_env;
  }
  std::string endpoint = absl::StrCat("https://localhost:", port);

  auto credentials = MakeAccessTokenCredentials(
      "dummy-token", std::chrono::system_clock::now() + std::chrono::hours(1));

  // Force HTTP/1.1 to make sure showcase multiplexer routes to REST.
  // Use https:// prefix for TLS.
  auto options =
      Options{}
          .set<EndpointOption>(endpoint)
          .set<CARootsFilePathOption>(ca_path)
          .set<UnifiedCredentialsOption>(credentials)
          .set<google::cloud::rest_internal::HttpVersionOption>("1.1");
  options = v1beta1_internal::EchoDefaultOptions(std::move(options));

  std::multimap<std::string, std::string> intercepted_headers;
  auto header_callback =
      [&intercepted_headers](
          std::multimap<std::string, std::string> const& headers) {
        intercepted_headers = headers;
      };

  // Create the real REST client
  auto real_client = rest_internal::MakePooledRestClient(endpoint, options);
  ASSERT_NE(real_client, nullptr) << "Failed to create real REST client";

  // Wrap it with our interceptor
  auto intercepting_client = std::make_shared<HeaderInterceptingRestClient>(
      std::move(real_client), header_callback);

  // Create DefaultEchoRestStub using the intercepting client
  std::shared_ptr<v1beta1_internal::EchoRestStub> stub =
      std::make_shared<v1beta1_internal::DefaultEchoRestStub>(
          intercepting_client, intercepting_client, options);

  // Decorate with metadata decorator (required for X-Goog-Api-Client header)
  // We must append "rest/" to the API client header to satisfy showcase REST
  // checks.
  std::string api_client_header = absl::StrCat(
      "rest/1.0.0 ", google::cloud::internal::GeneratedLibClientHeader());
  stub = std::make_shared<v1beta1_internal::EchoRestMetadata>(
      std::move(stub), std::move(api_client_header));

  // Create ConnectionImpl
  auto background = std::make_unique<
      rest_internal::AutomaticallyCreatedRestBackgroundThreads>();
  auto connection = std::make_shared<v1beta1_internal::EchoRestConnectionImpl>(
      std::move(background), std::move(stub), options);

  auto client = EchoClient(connection);

  ::google::showcase::v1beta1::EchoRequest request;
  request.set_content("Hello from C++ GAPIC REST!");

  auto response = client.Echo(request);
  ASSERT_TRUE(response.ok()) << response.status().message();
  EXPECT_EQ(response->content(), "Hello from C++ GAPIC REST!");

  // Verify headers
  auto get_header_value =
      [](std::multimap<std::string, std::string> const& headers,
         std::string const& key) -> std::string {
    for (auto const& pair : headers) {
      if (absl::EqualsIgnoreCase(pair.first, key)) {
        return pair.second;
      }
    }
    return "";
  };

  std::string tls_group =
      get_header_value(intercepted_headers, "x-showcase-tls-group");
  std::string supported_groups = get_header_value(
      intercepted_headers, "x-showcase-tls-client-supported-groups");

  EXPECT_FALSE(tls_group.empty()) << "x-showcase-tls-group header not found";
  EXPECT_FALSE(supported_groups.empty())
      << "x-showcase-tls-client-supported-groups header not found";

  // Assert PQC was used.
  EXPECT_EQ(tls_group, "X25519MLKEM768");
  EXPECT_TRUE(absl::StrContains(supported_groups, "X25519MLKEM768"));
}

}  // namespace
}  // namespace v1beta1
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
