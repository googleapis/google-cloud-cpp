// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_HTTP_REQUEST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_HTTP_REQUEST_H

#include "google/cloud/storage/internal/curl_handle_factory.h"
#include "google/cloud/storage/internal/curl_request.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <string>

namespace google {
namespace cloud {
namespace storage {
namespace testing {
/**
 * Wrap a googlemock mock into a move-assignable class.
 *
 * We need a class that is move-assignable because the tested code assumes the
 * object returned from MockHttpRequestBuilder::BuildRequest meets that
 * requirement.  Unfortunately, googlemock classes do not meet that requirement.
 * We use the PImpl idiom to wrap the class to meet the requirement.
 */
class MockHttpRequest {
 public:
  MockHttpRequest() : mock(std::make_shared<Impl>()) {}

  StatusOr<internal::HttpResponse> MakeRequest(std::string const& s) {
    return mock->MakeRequest(s);
  }

  struct Impl {
    MOCK_METHOD(StatusOr<storage::internal::HttpResponse>, MakeRequest,
                (std::string const&));
  };

  std::shared_ptr<Impl> mock;
};

/**
 * Mocks a `CurlRequestBuilder`.
 *
 * The structure of this mock is unusual. The classes under test create a
 * concrete instance of `CurlRequestBuilder`, mostly because (a) the class has
 * template member functions, so we cannot use a pure interface and a factor,
 * and (b) using a factory purely for testing seemed like overkill. Instead the
 * mock is implemented using a modified version of the PImpl idiom
 *
 * @see https://en.cppreference.com/w/cpp/language/pimpl
 */
class MockHttpRequestBuilder {
 public:
  explicit MockHttpRequestBuilder(
      // NOLINTNEXTLINE(performance-unnecessary-value-param)
      std::string url, std::shared_ptr<internal::CurlHandleFactory>) {
    mock_->Constructor(std::move(url));
  }

  using RequestType = MockHttpRequest;

  template <typename P>
  void AddWellKnownParameter(
      internal::WellKnownParameter<P, std::string> const& p) {
    if (p.has_value()) {
      mock_->AddQueryParameter(p.parameter_name(), p.value());
    }
  }

  template <typename P>
  void AddWellKnownParameter(
      internal::WellKnownParameter<P, std::int64_t> const& p) {
    if (p.has_value()) {
      mock_->AddQueryParameter(p.parameter_name(), std::to_string(p.value()));
    }
  }

  template <typename P>
  void AddWellKnownParameter(internal::WellKnownParameter<P, bool> const& p) {
    if (!p.has_value()) {
      return;
    }
    mock_->AddQueryParameter(p.parameter_name(), p.value() ? "true" : "false");
  }

  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  MockHttpRequest BuildRequest() { return mock_->BuildRequest(); }

  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  void AddUserAgentPrefix(std::string const& prefix) {
    mock_->AddUserAgentPrefix(prefix);
  }

  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  void AddHeader(std::string const& header) { mock_->AddHeader(header); }

  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  void AddQueryParameter(std::string const& key, std::string const& value) {
    mock_->AddQueryParameter(key, value);
  }

  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  void SetMethod(std::string const& method) { mock_->SetMethod(method); }

  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  void SetDebugLogging(bool enable) { mock_->SetDebugLogging(enable); }

  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  std::string UserAgentSuffix() { return mock_->UserAgentSuffix(); }

  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  std::unique_ptr<char[]> MakeEscapedString(std::string const& tmp) {
    return mock_->MakeEscapedString(tmp);
  }

  struct Impl {
    MOCK_METHOD(void, Constructor, (std::string));
    MOCK_METHOD(MockHttpRequest, BuildRequest, ());
    MOCK_METHOD(void, AddUserAgentPrefix, (std::string const&));
    MOCK_METHOD(void, AddHeader, (std::string const&));
    MOCK_METHOD(void, AddQueryParameter,
                (std::string const&, std::string const&));
    MOCK_METHOD(void, SetMethod, (std::string const&));
    MOCK_METHOD(void, SetDebugLogging, (bool));
    MOCK_METHOD(std::string, UserAgentSuffix, (), (const));
    MOCK_METHOD(std::unique_ptr<char[]>, MakeEscapedString,
                (std::string const&));
  };

  static std::shared_ptr<Impl> mock_;
};

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_HTTP_REQUEST_H
