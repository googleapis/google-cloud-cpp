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

#include "google/cloud/spanner/connection_options.h"
#include "google/cloud/spanner/internal/compiler_info.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/testing_util/environment_variable_restore.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using google::cloud::testing_util::EnvironmentVariableRestore;
using ::testing::HasSubstr;
using ::testing::StartsWith;

TEST(ConnectionOptionsTest, Credentials) {
  // In the CI environment grpc::GoogleDefaultCredentials() may assert. Use the
  // insecure credentials to initialize the options in any unit test.
  auto expected = grpc::InsecureChannelCredentials();
  ConnectionOptions options(expected);
  EXPECT_EQ(expected.get(), options.credentials().get());

  auto other_credentials = grpc::InsecureChannelCredentials();
  EXPECT_NE(expected, other_credentials);
  options.set_credentials(other_credentials);
  EXPECT_EQ(other_credentials, options.credentials());
}

TEST(ConnectionOptionsTest, AdminEndpoint) {
  ConnectionOptions options(grpc::InsecureChannelCredentials());
  EXPECT_EQ("spanner.googleapis.com", options.endpoint());
  options.set_endpoint("invalid-endpoint");
  EXPECT_EQ("invalid-endpoint", options.endpoint());
}

TEST(ConnectionOptionsTest, Tracing) {
  ConnectionOptions options(grpc::InsecureChannelCredentials());
  options.enable_tracing("fake-component");
  EXPECT_TRUE(options.tracing_enabled("fake-component"));
  options.disable_tracing("fake-component");
  EXPECT_FALSE(options.tracing_enabled("fake-component"));
}

TEST(ConnectionOptionsTest, DefaultTracingUnset) {
  EnvironmentVariableRestore restore("GOOGLE_CLOUD_CPP_ENABLE_TRACING");

  google::cloud::internal::UnsetEnv("GOOGLE_CLOUD_CPP_ENABLE_TRACING");
  ConnectionOptions options(grpc::InsecureChannelCredentials());
  EXPECT_FALSE(options.tracing_enabled("rpc"));
}

TEST(ConnectionOptionsTest, DefaultTracingSet) {
  EnvironmentVariableRestore restore("GOOGLE_CLOUD_CPP_ENABLE_TRACING");

  google::cloud::internal::SetEnv("GOOGLE_CLOUD_CPP_ENABLE_TRACING",
                                  "foo,bar,baz");
  ConnectionOptions options(grpc::InsecureChannelCredentials());
  EXPECT_FALSE(options.tracing_enabled("rpc"));
  EXPECT_TRUE(options.tracing_enabled("foo"));
  EXPECT_TRUE(options.tracing_enabled("bar"));
  EXPECT_TRUE(options.tracing_enabled("baz"));
}

TEST(ConnectionOptionsTest, ChannelPoolName) {
  ConnectionOptions options(grpc::InsecureChannelCredentials());
  EXPECT_TRUE(options.channel_pool_domain().empty());
  options.set_channel_pool_domain("test-channel-pool");
  EXPECT_EQ("test-channel-pool", options.channel_pool_domain());
}

TEST(ConnectionOptionsTest, BaseUserAgentPrefix) {
  auto actual = internal::BaseUserAgentPrefix();

  EXPECT_THAT(actual, StartsWith("gcloud-cpp/" + VersionString()));
  EXPECT_THAT(actual, HasSubstr(internal::CompilerId()));
  EXPECT_THAT(actual, HasSubstr(internal::CompilerVersion()));
  EXPECT_THAT(actual, HasSubstr(internal::CompilerFeatures()));
}

TEST(ConnectionOptionsTest, UserAgentPrefix) {
  ConnectionOptions options(grpc::InsecureChannelCredentials());
  EXPECT_EQ(internal::BaseUserAgentPrefix(), options.user_agent_prefix());
  options.add_user_agent_prefix("test-prefix/1.2.3");
  EXPECT_EQ("test-prefix/1.2.3 " + internal::BaseUserAgentPrefix(),
            options.user_agent_prefix());
}

TEST(ConnectionOptionsTest, CreateChannelArguments_Default) {
  ConnectionOptions options(grpc::InsecureChannelCredentials());

  auto actual = options.CreateChannelArguments();

  // Use the low-level C API because grpc::ChannelArguments lacks high-level
  // accessors.
  grpc_channel_args test_args = actual.c_channel_args();
  ASSERT_EQ(1, test_args.num_args);
  ASSERT_EQ(GRPC_ARG_STRING, test_args.args[0].type);
  EXPECT_EQ("grpc.primary_user_agent", std::string(test_args.args[0].key));
  // The gRPC library adds its own version to the user-agent string, so we only
  // check that our component appears in it.
  EXPECT_THAT(std::string(test_args.args[0].value.string),
              StartsWith(options.user_agent_prefix()));
}

TEST(ConnectionOptionsTest, CreateChannelArguments_WithChannelPool) {
  ConnectionOptions options(grpc::InsecureChannelCredentials());
  options.set_channel_pool_domain("testing-pool");
  options.add_user_agent_prefix("test-prefix/1.2.3");

  auto actual = options.CreateChannelArguments();

  // Use the low-level C API because grpc::ChannelArguments lacks high-level
  // accessors.
  grpc_channel_args test_args = actual.c_channel_args();
  ASSERT_EQ(2, test_args.num_args);
  ASSERT_EQ(GRPC_ARG_STRING, test_args.args[0].type);
  ASSERT_EQ(GRPC_ARG_STRING, test_args.args[1].type);

  // There is no (AFAICT) guarantee on the order of the arguments in this array,
  // and the C types are hard to work with. Capture the arguments in a map to
  // make it easier to work with them.
  std::map<std::string, std::string> args;
  for (std::size_t i = 0; i != test_args.num_args; ++i) {
    args[test_args.args[i].key] = test_args.args[i].value.string;
  }

  EXPECT_EQ("testing-pool", args["grpc.channel_pooling_domain"]);
  // The gRPC library adds its own version to the user-agent string, so we only
  // check that our component appears in it.
  EXPECT_THAT(args["grpc.primary_user_agent"],
              StartsWith(options.user_agent_prefix()));
}

TEST(ConnectionOptionsTest, DefaultBackgroundThreads) {
  auto options = ConnectionOptions(grpc::InsecureChannelCredentials());
  auto background = options.background_threads_factory()();

  using ms = std::chrono::milliseconds;

  // Verify the background thread is not this thread.
  auto background_thread_id = background->cq().MakeRelativeTimer(ms(0)).then(
      [](future<std::chrono::system_clock::time_point>) {
        return std::this_thread::get_id();
      });
  EXPECT_EQ(std::future_status::ready, background_thread_id.wait_for(ms(100)));

  auto actual = background_thread_id.get();
  EXPECT_NE(std::this_thread::get_id(), actual);
}

TEST(ConnectionOptionsTest, CustomBackgroundThreads) {
  grpc_utils::CompletionQueue cq;
  std::thread t([&cq] { cq.Run(); });

  auto options = ConnectionOptions(grpc::InsecureChannelCredentials())
                     .DisableBackgroundThreads(cq);
  auto background = options.background_threads_factory()();

  using ms = std::chrono::milliseconds;

  /// Verify the background thread is the thread provided above.
  auto background_thread_id = background->cq().MakeRelativeTimer(ms(0)).then(
      [](future<std::chrono::system_clock::time_point>) {
        return std::this_thread::get_id();
      });
  EXPECT_EQ(std::future_status::ready, background_thread_id.wait_for(ms(100)));

  auto actual = background_thread_id.get();
  EXPECT_EQ(t.get_id(), actual);

  cq.Shutdown();
  t.join();
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
