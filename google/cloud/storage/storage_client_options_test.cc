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

#include "google/cloud/internal/setenv.h"
#include "google/cloud/storage/client_options.h"
#include "google/cloud/testing_util/environment_variable_restore.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
class ClientOptionsTest : public ::testing::Test {
 public:
  ClientOptionsTest()
      : enable_tracing_("CLOUD_STORAGE_ENABLE_TRACING"),
        endpoint_("CLOUD_STORAGE_TESTBENCH_ENDPOINT") {}

 protected:
  void SetUp() override { enable_tracing_.SetUp(); }
  void TearDown() override { enable_tracing_.TearDown(); }

 protected:
  testing_util::EnvironmentVariableRestore enable_tracing_;
  testing_util::EnvironmentVariableRestore endpoint_;
};

TEST_F(ClientOptionsTest, Default) {
  google::cloud::internal::SetEnv("CLOUD_STORAGE_ENABLE_TRACING", nullptr);
  // Create the options with the insecure credentials because the default
  // credentials try to load the application default credentials, and those do
  // not exist in the CI environment, which results in errors or warnings.
  auto creds = CreateInsecureCredentials();
  ClientOptions options(creds);
  EXPECT_FALSE(options.enable_http_tracing());
  EXPECT_FALSE(options.enable_raw_client_tracing());
  EXPECT_TRUE(creds.get() == options.credentials().get());
  EXPECT_EQ("https://www.googleapis.com", options.endpoint());
  EXPECT_EQ("v1", options.version());
}

TEST_F(ClientOptionsTest, EnableRpc) {
  google::cloud::internal::SetEnv("CLOUD_STORAGE_ENABLE_TRACING",
                                  "foo,raw-client,bar");
  ClientOptions options(CreateInsecureCredentials());
  EXPECT_TRUE(options.enable_raw_client_tracing());
}

TEST_F(ClientOptionsTest, EnableHttp) {
  google::cloud::internal::SetEnv("CLOUD_STORAGE_ENABLE_TRACING",
                                  "foo,http,bar");
  ClientOptions options(CreateInsecureCredentials());
  EXPECT_TRUE(options.enable_http_tracing());
}

TEST_F(ClientOptionsTest, EndpointFromEnvironment) {
  google::cloud::internal::SetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT",
                                  "http://localhost:1234");
  ClientOptions options(CreateInsecureCredentials());
  EXPECT_EQ("http://localhost:1234", options.endpoint());
}

TEST_F(ClientOptionsTest, SetVersion) {
  ClientOptions options(CreateInsecureCredentials());
  options.set_version("vTest");
  EXPECT_EQ("vTest", options.version());
}

TEST_F(ClientOptionsTest, SetEndpoint) {
  ClientOptions options(CreateInsecureCredentials());
  options.set_endpoint("http://localhost:2345");
  EXPECT_EQ("http://localhost:2345", options.endpoint());
}

TEST_F(ClientOptionsTest, SetCredentials) {
  auto creds = CreateInsecureCredentials();
  ClientOptions options(creds);
  auto other = CreateInsecureCredentials();
  options.set_credentials(other);
  EXPECT_TRUE(other.get() == options.credentials().get());
  EXPECT_FALSE(creds.get() == other.get());
}

TEST_F(ClientOptionsTest, ProjectIdFromEnvironment) {
  google::cloud::internal::SetEnv("GOOGLE_CLOUD_PROJECT", "test-project-id");
  ClientOptions options(CreateInsecureCredentials());
  EXPECT_EQ("test-project-id", options.project_id());
}

TEST_F(ClientOptionsTest, ProjectIdFromEnvironmentNotSet) {
  google::cloud::internal::UnsetEnv("GOOGLE_CLOUD_PROJECT");
  ClientOptions options(CreateInsecureCredentials());
  EXPECT_EQ("", options.project_id());
}

TEST_F(ClientOptionsTest, SetProjectId) {
  ClientOptions options(CreateInsecureCredentials());
  options.set_project_id("test-project-id");
  EXPECT_EQ("test-project-id", options.project_id());
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
