// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/client_options.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/testing_util/environment_variable_restore.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using google::cloud::testing_util::EnvironmentVariableRestore;

TEST(ClientOptionsTest, Credentials) {
  // In the CI environment grpc::GoogleDefaultCredentials() may assert. Use the
  // insecure credentials to initialize the options in any unit test.
  auto expected = grpc::InsecureChannelCredentials();
  ClientOptions options(expected);
  EXPECT_EQ(expected.get(), options.credentials().get());
}

TEST(ClientOptionsTest, AdminEndpoint) {
  ClientOptions options(grpc::InsecureChannelCredentials());
  EXPECT_EQ("spanner.googleapis.com", options.endpoint());
  options.set_endpoint("invalid-endpoint");
  EXPECT_EQ("invalid-endpoint", options.endpoint());
}

TEST(ClientOptionsTest, Clog) {
  ClientOptions options(grpc::InsecureChannelCredentials());
  options.enable_clog();
  EXPECT_TRUE(options.clog_enabled());
  options.disable_clog();
  EXPECT_FALSE(options.clog_enabled());
}

TEST(ClientOptionsTest, Tracing) {
  ClientOptions options(grpc::InsecureChannelCredentials());
  options.enable_tracing("fake-component");
  EXPECT_TRUE(options.tracing_enabled("fake-component"));
  options.disable_tracing("fake-component");
  EXPECT_FALSE(options.tracing_enabled("fake-component"));
}

TEST(ClientOptionsTest, DefaultClogUnset) {
  EnvironmentVariableRestore restore("GOOGLE_CLOUD_CPP_ENABLE_CLOG");

  google::cloud::internal::UnsetEnv("GOOGLE_CLOUD_CPP_ENABLE_CLOG");
  ClientOptions options(grpc::InsecureChannelCredentials());
  EXPECT_FALSE(options.clog_enabled());
}

TEST(ClientOptionsTest, DefaultClogSet) {
  EnvironmentVariableRestore restore("GOOGLE_CLOUD_CPP_ENABLE_CLOG");

  google::cloud::internal::SetEnv("GOOGLE_CLOUD_CPP_ENABLE_CLOG", "");
  ClientOptions options(grpc::InsecureChannelCredentials());
  EXPECT_TRUE(options.clog_enabled());
}

TEST(ClientOptionsTest, DefaultTracingUnset) {
  EnvironmentVariableRestore restore("GOOGLE_CLOUD_CPP_ENABLE_TRACING");

  google::cloud::internal::UnsetEnv("GOOGLE_CLOUD_CPP_ENABLE_TRACING");
  ClientOptions options(grpc::InsecureChannelCredentials());
  EXPECT_FALSE(options.tracing_enabled("rpc"));
}

TEST(ClientOptionsTest, DefaultTracingSet) {
  EnvironmentVariableRestore restore("GOOGLE_CLOUD_CPP_ENABLE_TRACING");

  google::cloud::internal::SetEnv("GOOGLE_CLOUD_CPP_ENABLE_TRACING",
                                  "foo,bar,baz");
  ClientOptions options(grpc::InsecureChannelCredentials());
  EXPECT_FALSE(options.tracing_enabled("rpc"));
  EXPECT_TRUE(options.tracing_enabled("foo"));
  EXPECT_TRUE(options.tracing_enabled("bar"));
  EXPECT_TRUE(options.tracing_enabled("baz"));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
