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

#include "google/cloud/storage/internal/curl_request.h"
#include "google/cloud/storage/internal/nljson.h"
#include <gmock/gmock.h>

namespace storage {
namespace testing {
/**
 * Mock an instance of `storage::internal::CurlRequest`.
 *
 * The mocking code is a bit strange.  The class under test creates a concrete
 * object, mostly because it seemed overly complex to have a factory and/or
 * pass the object as a pointer.  But mock classes do not play well with
 * copy or move constructors.  Sigh.  The "solution" is to create a concrete
 * class that delegate all calls to a dynamically created mock.
 */
class MockHttpRequestHandle {
 public:
  MOCK_METHOD1(AddHeader, void(std::string const&));
  MOCK_METHOD2(AddQueryParameter, void(std::string const&, std::string const&));
  MOCK_METHOD1(MakeEscapedString, std::unique_ptr<char[]>(std::string const&));
  MOCK_METHOD1(PrepareRequest, void(std::string const&));
  MOCK_METHOD0(MakeRequest, storage::internal::HttpResponse());

  /**
   * Setup the most common expectation for MakeEscapedString.
   *
   * In most tests, MakeEscapedString() is easier to mock with some minimal
   * behavior rather explicit results for each input.  This function provides a
   * simple way to setup that behavior.
   */
  void SetupMakeEscapedString();
};

/**
 * Mock `storage::internal::CurlRequest` including stack-allocated instances.
 */
class MockHttpRequest {
 public:
  explicit MockHttpRequest(std::string url) : url_(std::move(url)) {
    (void)Handle(url_);
  }

  static void Clear() { handles_.clear(); }

  static std::shared_ptr<MockHttpRequestHandle> Handle(std::string const& url);

  void AddHeader(std::string const& header);
  void AddQueryParameter(std::string const& name, std::string const& value);
  std::unique_ptr<char[]> MakeEscapedString(std::string const& x);
  void PrepareRequest(std::string const& payload);
  storage::internal::HttpResponse MakeRequest();

  template <typename P>
  void AddWellKnownParameter(WellKnownParameter<P, std::string> const& p) {
    if (p.has_value()) {
      handles_[url_]->AddQueryParameter(p.parameter_name(), p.value());
    }
  }

  template <typename P>
  void AddWellKnownParameter(WellKnownParameter<P, std::int64_t> const& p) {
    if (p.has_value()) {
      handles_[url_]->AddQueryParameter(p.parameter_name(),
                                        std::to_string(p.value()));
    }
  }

 private:
  std::string url_;
  static std::map<std::string, std::shared_ptr<MockHttpRequestHandle>> handles_;
};
}  // namespace testing
}  // namespace storage

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_HTTP_REQUEST_H_
