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
#include "google/cloud/storage/oauth2/google_credentials.h"
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
  // Create the options with the anonymous credentials because the default
  // credentials try to load the application default credentials, and those do
  // not exist in the CI environment, which results in errors or warnings.
  auto creds = oauth2::CreateAnonymousCredentials();
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
  ClientOptions options(oauth2::CreateAnonymousCredentials());
  EXPECT_TRUE(options.enable_raw_client_tracing());
}

TEST_F(ClientOptionsTest, EnableHttp) {
  google::cloud::internal::SetEnv("CLOUD_STORAGE_ENABLE_TRACING",
                                  "foo,http,bar");
  ClientOptions options(oauth2::CreateAnonymousCredentials());
  EXPECT_TRUE(options.enable_http_tracing());
}

TEST_F(ClientOptionsTest, EndpointFromEnvironment) {
  google::cloud::internal::SetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT",
                                  "http://localhost:1234");
  ClientOptions options(oauth2::CreateAnonymousCredentials());
  EXPECT_EQ("http://localhost:1234", options.endpoint());
}

TEST_F(ClientOptionsTest, SetVersion) {
  ClientOptions options(oauth2::CreateAnonymousCredentials());
  options.set_version("vTest");
  EXPECT_EQ("vTest", options.version());
}

TEST_F(ClientOptionsTest, SetEndpoint) {
  ClientOptions options(oauth2::CreateAnonymousCredentials());
  options.set_endpoint("http://localhost:2345");
  EXPECT_EQ("http://localhost:2345", options.endpoint());
}

TEST_F(ClientOptionsTest, SetCredentials) {
  auto creds = oauth2::CreateAnonymousCredentials();
  ClientOptions options(creds);
  auto other = oauth2::CreateAnonymousCredentials();
  options.set_credentials(other);
  EXPECT_TRUE(other.get() == options.credentials().get());
  EXPECT_FALSE(creds.get() == other.get());
}

TEST_F(ClientOptionsTest, ProjectIdFromEnvironment) {
  google::cloud::internal::SetEnv("GOOGLE_CLOUD_PROJECT", "test-project-id");
  ClientOptions options(oauth2::CreateAnonymousCredentials());
  EXPECT_EQ("test-project-id", options.project_id());
}

TEST_F(ClientOptionsTest, ProjectIdFromEnvironmentNotSet) {
  google::cloud::internal::UnsetEnv("GOOGLE_CLOUD_PROJECT");
  ClientOptions options(oauth2::CreateAnonymousCredentials());
  EXPECT_EQ("", options.project_id());
}

TEST_F(ClientOptionsTest, SetProjectId) {
  ClientOptions options(oauth2::CreateAnonymousCredentials());
  options.set_project_id("test-project-id");
  EXPECT_EQ("test-project-id", options.project_id());
}

TEST_F(ClientOptionsTest, SetdownloadBufferSize) {
  ClientOptions client_options;
  auto default_size = client_options.download_buffer_size();
  EXPECT_NE(0U, default_size);
  client_options.SetDownloadBufferSize(1024);
  EXPECT_EQ(1024U, client_options.download_buffer_size());
  client_options.SetDownloadBufferSize(0);
  EXPECT_EQ(default_size, client_options.download_buffer_size());
}

TEST_F(ClientOptionsTest, SetUploadBufferSize) {
  ClientOptions client_options;
  auto default_size = client_options.upload_buffer_size();
  EXPECT_NE(0U, default_size);
  client_options.SetUploadBufferSize(1024);
  EXPECT_EQ(1024U, client_options.upload_buffer_size());
  client_options.SetUploadBufferSize(0);
  EXPECT_EQ(default_size, client_options.upload_buffer_size());
}

TEST_F(ClientOptionsTest, UserAgentPrefix) {
  ClientOptions options(oauth2::CreateAnonymousCredentials());
  EXPECT_EQ("", options.user_agent_prefix());
  options.add_user_agent_prefx("foo-1.0");
  EXPECT_EQ("foo-1.0", options.user_agent_prefix());
  options.add_user_agent_prefx("bar-2.2");
  EXPECT_EQ("bar-2.2/foo-1.0", options.user_agent_prefix());
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
