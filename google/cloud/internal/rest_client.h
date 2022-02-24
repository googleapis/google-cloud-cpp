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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_CLIENT_H

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

// Provides factory function to create a CurlRestClient that does not manage a
// pool of connections.
std::unique_ptr<RestClient> MakeDefaultRestClient(std::string endpoint_address,
                                                  Options options);

// Provides factory function to create a CurlRestClient that manages a pool of
// connections which are reused in order to minimize costs of setup and
// teardown.
std::unique_ptr<RestClient> MakePooledRestClient(std::string endpoint_address,
                                                 Options options);

// Provides methods corresponding to HTTP verbs to make requests to RESTful
// services.
class RestClient {
 public:
  virtual ~RestClient() = default;
  virtual StatusOr<std::unique_ptr<RestResponse>> Delete(
      RestRequest const& request) = 0;
  virtual StatusOr<std::unique_ptr<RestResponse>> Get(
      RestRequest const& request) = 0;
  virtual StatusOr<std::unique_ptr<RestResponse>> Patch(
      RestRequest const& request,
      std::vector<absl::Span<char const>> const& payload) = 0;
  virtual StatusOr<std::unique_ptr<RestResponse>> Post(
      RestRequest const& request,
      std::vector<absl::Span<char const>> const& payload) = 0;
  virtual StatusOr<std::unique_ptr<RestResponse>> Post(
      RestRequest request,
      std::vector<std::pair<std::string, std::string>> const& form_data) = 0;
  virtual StatusOr<std::unique_ptr<RestResponse>> Put(
      RestRequest const& request,
      std::vector<absl::Span<char const>> const& payload) = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_CLIENT_H
