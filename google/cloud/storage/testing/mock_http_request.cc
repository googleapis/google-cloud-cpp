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

#include "google/cloud/storage/testing/mock_http_request.h"

namespace storage {
namespace testing {
void MockHttpRequestHandle::SetupMakeEscapedString() {
  using namespace ::testing;
  EXPECT_CALL(*this, MakeEscapedString(_))
      .WillRepeatedly(Invoke([](std::string const& x) {
        auto const size = x.size();
        auto copy = new char[size + 1];
        std::memcpy(copy, x.data(), x.size());
        copy[size] = '\0';
        return std::unique_ptr<char[]>(copy);
      }));
}

std::map<std::string, std::shared_ptr<MockHttpRequestHandle>>
    MockHttpRequest::handles_;

std::shared_ptr<MockHttpRequestHandle> MockHttpRequest::Handle(
    std::string const& url) {
  auto ins = handles_.emplace(url, std::shared_ptr<MockHttpRequestHandle>());
  if (ins.second) {
    // If successfully inserted then create the object, cheaper this way.
    ins.first->second = std::make_shared<MockHttpRequestHandle>();
  }
  return ins.first->second;
}

void MockHttpRequest::AddHeader(std::string const& key,
                                std::string const& value) {
  handles_[url_]->AddHeader(key, value);
}
void MockHttpRequest::AddHeader(std::string const& header) {
  handles_[url_]->AddHeader(header);
}
void MockHttpRequest::AddQueryParameter(std::string const& name,
                                        std::string const& value) {
  handles_[url_]->AddQueryParameter(name, value);
}
std::unique_ptr<char[]> MockHttpRequest::MakeEscapedString(
    std::string const& x) {
  return handles_[url_]->MakeEscapedString(x);
}
void MockHttpRequest::PrepareRequest(std::string const& payload) {
  handles_[url_]->PrepareRequest(payload);
}
void MockHttpRequest::PrepareRequest(storage::internal::nl::json json) {
  handles_[url_]->PrepareRequest(std::move(json));
}
storage::internal::HttpResponse MockHttpRequest::MakeRequest() {
  return handles_[url_]->MakeRequest();
}

}  // namespace testing
}  // namespace storage
