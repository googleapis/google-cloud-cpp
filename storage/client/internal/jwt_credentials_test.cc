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

#include "storage/client/internal/jwt_credentials.h"
#include <gmock/gmock.h>

// The mocking code is a bit strange.  The class under test creates a concrete
// object, mostly because it seemed overly complex to have a factory and/or
// pass the object as a pointer.  But mock classes do not play well with
// copy or move constructors.  Sigh.  The "solution" is to create a concrete
// class that delegate all calls to a dynamically created mock.
class MockHttpRequestHandle {
 public:
  MOCK_METHOD2(AddHeader, void(std::string const&, std::string const&));
  MOCK_METHOD1(AddHeader, void(std::string const&));
  MOCK_METHOD2(AddQueryParameter, void(std::string const&, std::string const&));
  MOCK_METHOD1(MakeEscapedString, std::unique_ptr<char[]>(std::string const&));
  MOCK_METHOD1(PrepareRequest, void(std::string const&));
  MOCK_METHOD1(PrepareRequest, void(storage::internal::nl::json));
  MOCK_METHOD0(MakeRequest, std::string());
};

class MockHttpRequest {
 public:
  MockHttpRequest(std::string url) : url_(std::move(url)) {
    (void)Handle(url_);
  }

  static void Clear() { handles_.clear(); }

  static std::shared_ptr<MockHttpRequestHandle> Handle(std::string const& url) {
    auto ins = handles_.emplace(url, std::shared_ptr<MockHttpRequestHandle>());
    if (ins.second) {
      // If successfully inserted then create the object, cheaper this way.
      ins.first->second = std::make_shared<MockHttpRequestHandle>();
    }
    return ins.first->second;
  }

  void AddHeader(std::string const& key, std::string const& value) {
    handles_[url_]->AddHeader(key, value);
  }
  void AddHeader(std::string const& header) {
    handles_[url_]->AddHeader(header);
  }
  void AddQueryParameter(std::string const& name, std::string const& value) {
    handles_[url_]->AddQueryParameter(name, value);
  }
  std::unique_ptr<char[]> MakeEscapedString(std::string const& x) {
    return handles_[url_]->MakeEscapedString(x);
  }
  void PrepareRequest(std::string const& payload) {
    handles_[url_]->PrepareRequest(payload);
  }
  void PrepareRequest(storage::internal::nl::json json) {
    handles_[url_]->PrepareRequest(std::move(json));
  }
  std::string MakeRequest() { return handles_[url_]->MakeRequest(); }

 private:
  std::string url_;
  static std::map<std::string, std::shared_ptr<MockHttpRequestHandle>> handles_;
};

std::map<std::string, std::shared_ptr<MockHttpRequestHandle>>
    MockHttpRequest::handles_;

class JwtCredentialsTest : public ::testing::Test {
 protected:
  void SetUp() { MockHttpRequest::Clear(); }
  void TearDown() { MockHttpRequest::Clear(); }
};

/// @test Verify that we can create credentials from a JWT string.
TEST_F(JwtCredentialsTest, Simple) {
  std::string jwt = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";

  using namespace ::testing;
  using storage::internal::JwtCredentials;

  auto handle =
      MockHttpRequest::Handle(storage::internal::GOOGLE_OAUTH_REFRESH_URL);
  EXPECT_CALL(*handle, PrepareRequest(An<std::string const&>()))
      .WillOnce(Invoke([](std::string const& payload) {
        auto npos = std::string::npos;
        EXPECT_NE(npos, payload.find("grant_type=refresh_token"));
        EXPECT_NE(npos, payload.find("client_id=a-client-id.example.com"));
        EXPECT_NE(npos, payload.find("client_secret=a-123456ABCDEF"));
        EXPECT_NE(npos, payload.find("refresh_token=1/THETOKEN"));
      }));
  EXPECT_CALL(*handle, MakeEscapedString(_))
      .WillRepeatedly(Invoke([](std::string const& x) {
        auto const size = x.size();
        auto copy = new char[size + 1];
        std::memcpy(copy, x.data(), x.size());
        copy[size] = '\0';
        return std::unique_ptr<char[]>(copy);
      }));

  std::string response = R"""({
    "token_type": "Type",
    "access_token": "access-token-value",
    "id_token": "id-token-value",
    "expires_in": 1234
})""";
  EXPECT_CALL(*handle, MakeRequest()).WillOnce(Return(response));

  JwtCredentials<MockHttpRequest> credentials(jwt);
  EXPECT_EQ("Type access-token-value", credentials.AuthorizationHeader());
}
