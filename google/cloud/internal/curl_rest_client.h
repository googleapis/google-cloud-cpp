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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_REST_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_REST_CLIENT_H

#include "google/cloud/internal/rest_client.h"
#include "google/cloud/internal/rest_options.h"
#include "google/cloud/internal/rest_request.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class CurlHandleFactory;
class CurlImpl;

// RestClient implementation using libcurl. In order to maximize the performance
// of the connection that libcurl manages, the endpoint that the client connects
// to cannot be changed after creation. If a service needs to communicate with
// multiple endpoints, use a different CurlRestClient for each such endpoint.
class CurlRestClient : public RestClient {
 public:
  static std::string HostHeader(Options const& options,
                                std::string const& endpoint);
  ~CurlRestClient() override = default;

  CurlRestClient(CurlRestClient const&) = delete;
  CurlRestClient(CurlRestClient&&) = default;
  CurlRestClient& operator=(CurlRestClient const&) = delete;
  CurlRestClient& operator=(CurlRestClient&&) = default;

  StatusOr<std::unique_ptr<RestResponse>> Delete(
      RestRequest const& request) override;
  StatusOr<std::unique_ptr<RestResponse>> Get(
      RestRequest const& request) override;
  StatusOr<std::unique_ptr<RestResponse>> Patch(
      RestRequest const& request,
      std::vector<absl::Span<char const>> const& payload) override;
  StatusOr<std::unique_ptr<RestResponse>> Post(
      RestRequest const& request,
      std::vector<absl::Span<char const>> const& payload) override;
  StatusOr<std::unique_ptr<RestResponse>> Post(
      RestRequest request,
      std::vector<std::pair<std::string, std::string>> const& form_data)
      override;
  StatusOr<std::unique_ptr<RestResponse>> Put(
      RestRequest const& request,
      std::vector<absl::Span<char const>> const& payload) override;

 private:
  friend std::unique_ptr<RestClient> MakeDefaultRestClient(
      std::string endpoint_address, Options options);

  friend class std::unique_ptr<RestClient> MakePooledRestClient(
      std::string endpoint_address, Options options);

  CurlRestClient(std::string endpoint_address,
                 std::shared_ptr<CurlHandleFactory> factory, Options options);
  StatusOr<std::unique_ptr<CurlImpl>> CreateCurlImpl(
      RestRequest const& request);

  std::string endpoint_address_;
  std::shared_ptr<CurlHandleFactory> handle_factory_;
  std::string x_goog_api_client_header_;
  Options options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_REST_CLIENT_H
