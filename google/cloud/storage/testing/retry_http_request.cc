// Copyright 2023 Google LLC
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

#include "google/cloud/storage/testing/retry_http_request.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_client.h"
#include <chrono>
#include <thread>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
namespace testing {
namespace {

StatusOr<std::string> HandleResponse(
    StatusOr<std::unique_ptr<rest_internal::RestResponse>> r) {
  if (!r) return std::move(r).status();
  auto response = *std::move(r);
  if (response->StatusCode() != 200) {
    return internal::InternalError(
        "unexpected status code",
        GCP_ERROR_INFO().WithMetadata("http.status_code",
                                      std::to_string(response->StatusCode())));
  }
  return rest_internal::ReadAll(std::move(*response).ExtractPayload());
}

StatusOr<std::string> Retry(
    std::function<StatusOr<std::string>()> const& call) {
  StatusOr<std::string> response;
  for (int i = 0; i != 3; ++i) {
    response = call();
    if (response) return response;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return response;
}

}  // namespace

StatusOr<std::string> RetryHttpGet(
    std::string const& url,
    std::function<rest_internal::RestRequest()> const& factory) {
  auto client = rest_internal::MakeDefaultRestClient(
      url, Options{}.set<LoggingComponentsOption>({"http"}));
  auto call = [&]() -> StatusOr<std::string> {
    rest_internal::RestContext context;
    return HandleResponse(client->Get(context, factory()));
  };
  return Retry(call);
}

StatusOr<std::string> RetryHttpPut(
    std::string const& url,
    std::function<rest_internal::RestRequest()> const& factory,
    std::string const& payload) {
  auto client = rest_internal::MakeDefaultRestClient(
      url, Options{}.set<LoggingComponentsOption>({"http"}));
  auto call = [&]() -> StatusOr<std::string> {
    rest_internal::RestContext context;
    return HandleResponse(
        client->Put(context, factory(), {absl::Span<char const>(payload)}));
  };
  return Retry(call);
}

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google
