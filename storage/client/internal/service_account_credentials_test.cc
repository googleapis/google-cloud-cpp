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

#include "storage/client/internal/service_account_credentials.h"
#include "storage/client/testing/mock_http_request.h"
#include "google/cloud/internal/setenv.h"
#include <gmock/gmock.h>

using storage::testing::MockHttpRequest;

class ServiceAccountCredentialsTest : public ::testing::Test {
 protected:
  void SetUp() { MockHttpRequest::Clear(); }
  void TearDown() { MockHttpRequest::Clear(); }
};

using namespace ::testing;
using storage::internal::ServiceAccountCredentials;

/// @test Verify that we can create credentials from a JWT string.
TEST_F(ServiceAccountCredentialsTest, Simple) {
  std::string jwt = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";

  auto handle =
      MockHttpRequest::Handle(storage::internal::GOOGLE_OAUTH_REFRESH_ENDPOINT);
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
  EXPECT_CALL(*handle, MakeRequest())
      .WillOnce(Return(storage::internal::HttpResponse{200, response, {}}));

  ServiceAccountCredentials<MockHttpRequest> credentials(jwt);
  EXPECT_EQ("Type access-token-value", credentials.AuthorizationHeader());
}

/// @test Verify that we can refresh service account credentials.
TEST_F(ServiceAccountCredentialsTest, Refresh) {
  std::string jwt = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";

  auto handle =
      MockHttpRequest::Handle(storage::internal::GOOGLE_OAUTH_REFRESH_ENDPOINT);
  EXPECT_CALL(*handle, PrepareRequest(An<std::string const&>())).Times(1);
  EXPECT_CALL(*handle, MakeEscapedString(_))
      .WillRepeatedly(Invoke([](std::string const& x) {
        auto const size = x.size();
        auto copy = new char[size + 1];
        std::memcpy(copy, x.data(), x.size());
        copy[size] = '\0';
        return std::unique_ptr<char[]>(copy);
      }));

  // Prepare two responses, the first one is used but becomes immediately
  // expired.
  std::string r1 = R"""({
    "token_type": "Type",
    "access_token": "access-token-r1",
    "id_token": "id-token-value",
    "expires_in": 0
})""";
  std::string r2 = R"""({
    "token_type": "Type",
    "access_token": "access-token-r2",
    "id_token": "id-token-value",
    "expires_in": 1000
})""";
  EXPECT_CALL(*handle, MakeRequest())
      .WillOnce(Return(storage::internal::HttpResponse{200, r1, {}}))
      .WillOnce(Return(storage::internal::HttpResponse{200, r2, {}}));

  ServiceAccountCredentials<MockHttpRequest> credentials(jwt);
  EXPECT_EQ("Type access-token-r1", credentials.AuthorizationHeader());
  EXPECT_EQ("Type access-token-r2", credentials.AuthorizationHeader());
  EXPECT_EQ("Type access-token-r2", credentials.AuthorizationHeader());
}

class EnvironmentVariableRestore {
 public:
  explicit EnvironmentVariableRestore(char const* variable_name)
      : EnvironmentVariableRestore(std::string(variable_name)) {}

  explicit EnvironmentVariableRestore(std::string variable_name)
      : variable_name_(std::move(variable_name)) {}

  void SetUp() { previous_ = std::getenv(variable_name_.c_str()); }
  void TearDown() {
    google::cloud::internal::SetEnv(variable_name_.c_str(), previous_);
  }

 private:
  std::string variable_name_;
  char const* previous_;
};

class DefaultServiceAccountFileTest : public ::testing::Test {
 public:
  DefaultServiceAccountFileTest()
      : home_(
            storage::internal::DefaultServiceAccountCredentialsHomeVariable()),
        override_variable_("GOOGLE_APPLICATION_CREDENTIALS") {}

 protected:
  void SetUp() override {
    home_.SetUp();
    override_variable_.SetUp();
  }
  void TearDown() override {
    override_variable_.TearDown();
    home_.TearDown();
  }

 protected:
  EnvironmentVariableRestore home_;
  EnvironmentVariableRestore override_variable_;
};

/// @test Verify that the application can override the default credentials.
TEST_F(DefaultServiceAccountFileTest, EnvironmentVariableSet) {
  google::cloud::internal::SetEnv("GOOGLE_APPLICATION_CREDENTIALS",
                                  "/foo/bar/baz");
  auto actual = storage::internal::DefaultServiceAccountCredentialsFile();
  EXPECT_EQ("/foo/bar/baz", actual);
}

/// @test Verify that the file path works as expected when using HOME.
TEST_F(DefaultServiceAccountFileTest, HomeSet) {
  google::cloud::internal::SetEnv("GOOGLE_APPLICATION_CREDENTIALS", nullptr);
  char const* home =
      storage::internal::DefaultServiceAccountCredentialsHomeVariable();
  google::cloud::internal::SetEnv(home, "/foo/bar/baz");
  auto actual = storage::internal::DefaultServiceAccountCredentialsFile();
  EXPECT_NE(std::string::npos, actual.find("/foo/bar/baz"));
  EXPECT_NE(std::string::npos, actual.find(".json"));
}

/// @test Verify that the service account file path fails when HOME is not set.
TEST_F(DefaultServiceAccountFileTest, HomeNotSet) {
  google::cloud::internal::SetEnv("GOOGLE_APPLICATION_CREDENTIALS", nullptr);
  char const* home =
      storage::internal::DefaultServiceAccountCredentialsHomeVariable();
  google::cloud::internal::UnsetEnv(home);
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(storage::internal::DefaultServiceAccountCredentialsFile(),
               std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      storage::internal::DefaultServiceAccountCredentialsFile(),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}
