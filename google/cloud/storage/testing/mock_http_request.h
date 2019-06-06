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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_HTTP_REQUEST_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_HTTP_REQUEST_H_

#include "google/cloud/storage/internal/curl_handle_factory.h"
#include "google/cloud/storage/internal/curl_request.h"
#include "google/cloud/storage/internal/nljson.h"
#include <gmock/gmock.h>

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
    MOCK_METHOD1(MakeRequest,
                 StatusOr<storage::internal::HttpResponse>(std::string const&));
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
      std::string url, std::shared_ptr<internal::CurlHandleFactory>) {
    mock->Constructor(std::move(url));
  }

  using RequestType = MockHttpRequest;

  template <typename P>
  void AddWellKnownParameter(
      internal::WellKnownParameter<P, std::string> const& p) {
    if (p.has_value()) {
      mock->AddQueryParameter(p.parameter_name(), p.value());
    }
  }

  template <typename P>
  void AddWellKnownParameter(
      internal::WellKnownParameter<P, std::int64_t> const& p) {
    if (p.has_value()) {
      mock->AddQueryParameter(p.parameter_name(), std::to_string(p.value()));
    }
  }

  template <typename P>
  void AddWellKnownParameter(internal::WellKnownParameter<P, bool> const& p) {
    if (!p.has_value()) {
      return;
    }
    mock->AddQueryParameter(p.parameter_name(), p.value() ? "true" : "false");
  }

  MockHttpRequest BuildRequest() { return mock->BuildRequest(); }

  void AddUserAgentPrefix(std::string const& prefix) {
    mock->AddUserAgentPrefix(prefix);
  }

  void AddHeader(std::string const& header) { mock->AddHeader(header); }

  void AddQueryParameter(std::string const& key, std::string const& value) {
    mock->AddQueryParameter(key, value);
  }

  void SetMethod(std::string const& method) { mock->SetMethod(method); }

  void SetDebugLogging(bool enable) { mock->SetDebugLogging(enable); }

  std::string UserAgentSuffix() { return mock->UserAgentSuffix(); }

  std::unique_ptr<char[]> MakeEscapedString(std::string const& tmp) {
    return mock->MakeEscapedString(tmp);
  }

  struct Impl {
    MOCK_METHOD1(Constructor, void(std::string));
    MOCK_METHOD0(BuildRequest, MockHttpRequest());
    MOCK_METHOD1(AddUserAgentPrefix, void(std::string const&));
    MOCK_METHOD1(AddHeader, void(std::string const&));
    MOCK_METHOD2(AddQueryParameter,
                 void(std::string const&, std::string const&));
    MOCK_METHOD1(SetMethod, void(std::string const&));
    MOCK_METHOD1(SetDebugLogging, void(bool));
    MOCK_CONST_METHOD0(UserAgentSuffix, std::string());
    MOCK_METHOD1(MakeEscapedString,
                 std::unique_ptr<char[]>(std::string const&));
  };

  static std::shared_ptr<Impl> mock;
};

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_HTTP_REQUEST_H_
