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

#include "google/cloud/internal/rest_context.h"
#include "google/cloud/internal/rest_options.h"
#include "google/cloud/internal/rest_request.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/options.h"
#include "google/cloud/rest_options.h"
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

/**
 * Provides methods corresponding to HTTP verbs to make HTTP requests.
 *
 * Concrete versions of this class make HTTP requests. While typically used with
 * RESTful services, the interface and implementations can be used to make any
 * HTTP requests.
 *
 * The headers, payload, and query parameters for the request are passed in as
 * a `RestRequest` parameter.  The result is a
 * `StatusOr<std::unique_ptr<RestResponse>>`. On success, the `RestResponse`
 * contains the HTTP status code, response headers, and an object to iterate
 * over the payload.
 *
 * Note that HTTP requests that fail with an HTTP status code, e.g. with
 * "404 - NOT FOUND", are considered a success. That is, the returned
 * `StatusOr<>` will contain a value (and not an error). Callers can convert
 * these HTTP errors to a `Status` using @ref AsStatus(RestResponse&&). In some
 * cases (e.g. PUT requests for GCS resumable uploads) an HTTP error is
 * "normal", and should be treated as a successful request.
 *
 * Each method consumes a `RestContext` parameter. Often the `request` parameter
 * is prepared once as part of a retry loop. The `RestContext` can be used to
 * provide or change headers in retry, tracing, or other decorators. The
 * `RestContext` also returns request metadata, such as the local and remote
 * IP and port. Such metadata is useful for tracing and troubleshooting.
 */
class RestClient {
 public:
  virtual ~RestClient() = default;
  virtual StatusOr<std::unique_ptr<RestResponse>> Delete(
      RestContext& context, RestRequest const& request) = 0;
  virtual StatusOr<std::unique_ptr<RestResponse>> Get(
      RestContext& context, RestRequest const& request) = 0;
  virtual StatusOr<std::unique_ptr<RestResponse>> Patch(
      RestContext& context, RestRequest const& request,
      std::vector<absl::Span<char const>> const& payload) = 0;
  virtual StatusOr<std::unique_ptr<RestResponse>> Post(
      RestContext& context, RestRequest const& request,
      std::vector<absl::Span<char const>> const& payload) = 0;
  virtual StatusOr<std::unique_ptr<RestResponse>> Post(
      RestContext& context, RestRequest const& request,
      std::vector<std::pair<std::string, std::string>> const& form_data) = 0;
  virtual StatusOr<std::unique_ptr<RestResponse>> Put(
      RestContext& context, RestRequest const& request,
      std::vector<absl::Span<char const>> const& payload) = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_CLIENT_H
