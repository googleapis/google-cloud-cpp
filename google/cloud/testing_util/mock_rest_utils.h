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
//
#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_REST_UTILS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_REST_UTILS_H

#include "google/cloud/credentials.h"
#include "google/cloud/internal/http_payload.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

class MockRestClient : public rest_internal::RestClient {
 public:
  static std::unique_ptr<RestClient> GetRestClient(std::string endpoint,
                                                   Options options) {
    return std::unique_ptr<MockRestClient>(
        new MockRestClient(std::move(endpoint), std::move(options)));
  }

  MockRestClient(std::string endpoint, Options options)
      : endpoint_(std::move(endpoint)),
        options_(std::move(options)),
        impl_(SingleImpl()) {
    impl_->Constructor(endpoint_, options_);
  }

  StatusOr<std::unique_ptr<rest_internal::RestResponse>> Delete(
      rest_internal::RestRequest const& request) override {
    return impl_->Delete(request);
  }
  StatusOr<std::unique_ptr<rest_internal::RestResponse>> Get(
      rest_internal::RestRequest const& request) override {
    return impl_->Get(request);
  }
  StatusOr<std::unique_ptr<rest_internal::RestResponse>> Patch(
      rest_internal::RestRequest const& request,
      std::vector<absl::Span<char const>> const& payload) override {
    return impl_->Patch(request, payload);
  }
  StatusOr<std::unique_ptr<rest_internal::RestResponse>> Post(
      rest_internal::RestRequest const& request,
      std::vector<absl::Span<char const>> const& payload) override {
    return impl_->Post(request, payload);
  }
  StatusOr<std::unique_ptr<rest_internal::RestResponse>> Put(
      rest_internal::RestRequest const& request,
      std::vector<absl::Span<char const>> const& payload) override {
    return impl_->Put(request, payload);
  }

  struct Impl : public RestClient {
    ~Impl() override = default;
    //    MOCK_METHOD(void, Constructor, (std::string,
    //    absl::optional<std::string>));
    MOCK_METHOD(void, Constructor, (std::string, Options));
    MOCK_METHOD(StatusOr<std::unique_ptr<rest_internal::RestResponse>>, Delete,
                (rest_internal::RestRequest const& request), (override));
    MOCK_METHOD(StatusOr<std::unique_ptr<rest_internal::RestResponse>>, Get,
                (rest_internal::RestRequest const& request), (override));
    MOCK_METHOD(StatusOr<std::unique_ptr<rest_internal::RestResponse>>, Patch,
                (rest_internal::RestRequest const& request,
                 std::vector<absl::Span<char const>> const& payload),
                (override));
    MOCK_METHOD(StatusOr<std::unique_ptr<rest_internal::RestResponse>>, Post,
                (rest_internal::RestRequest const& request,
                 std::vector<absl::Span<char const>> const& payload),
                (override));
    MOCK_METHOD(StatusOr<std::unique_ptr<rest_internal::RestResponse>>, Put,
                (rest_internal::RestRequest const& request,
                 std::vector<absl::Span<char const>> const& payload),
                (override));
  };

  static std::shared_ptr<Impl> SingleImpl() {
    static std::shared_ptr<Impl> k_impl(new Impl());
    return k_impl;
  }

  std::string endpoint_;
  Options options_;
  std::shared_ptr<Impl> impl_;
};

class MockRestResponse : public rest_internal::RestResponse {
 public:
  ~MockRestResponse() override = default;
  MOCK_METHOD(rest_internal::HttpStatusCode, StatusCode, (), (const, override));
  MOCK_METHOD((std::multimap<std::string, std::string>), Headers, (),
              (const, override));
  MOCK_METHOD(std::unique_ptr<rest_internal::HttpPayload>, ExtractPayload, (),
              (ref(&&), override));
};

class MockHttpPayload : public rest_internal::HttpPayload {
 public:
  ~MockHttpPayload() override = default;
  MOCK_METHOD(StatusOr<std::size_t>, Read, (absl::Span<char> buffer),
              (override));
  MOCK_METHOD((std::multimap<std::string, std::string>), Trailers, (),
              (const, override));
};

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_REST_UTILS_H
