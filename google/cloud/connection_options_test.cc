// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/connection_options.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/compiler_info.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>
#include <map>

// This test disables all deprecation warnings because it is supposed to test a
// deprecated (but not retired) class.
#include "google/cloud/internal/disable_deprecation_warnings.inc"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::ElementsAre;
using ::testing::StartsWith;
using ::testing::UnorderedElementsAre;

/// Use these traits to test ConnectionOptions.
struct TestTraits {
  static std::string default_endpoint() { return "test-endpoint.example.com"; }
  static std::string user_agent_prefix() { return "test-prefix"; }
  static int default_num_channels() { return 7; }
};

using TestConnectionOptions = ConnectionOptions<TestTraits>;

TEST(ConnectionOptionsTest, Credentials) {
  // In the CI environment grpc::GoogleDefaultCredentials() may assert. Use the
  // insecure credentials to initialize the conn_opts in any unit test.
  auto expected = grpc::InsecureChannelCredentials();
  TestConnectionOptions conn_opts(expected);
  EXPECT_EQ(expected.get(), conn_opts.credentials().get());
  EXPECT_EQ(expected,
            internal::MakeOptions(conn_opts).get<GrpcCredentialOption>());

  auto other_credentials = grpc::InsecureChannelCredentials();
  EXPECT_NE(expected, other_credentials);
  conn_opts.set_credentials(other_credentials);
  EXPECT_EQ(other_credentials, conn_opts.credentials());
  EXPECT_EQ(other_credentials,
            internal::MakeOptions(conn_opts).get<GrpcCredentialOption>());
}

TEST(ConnectionOptionsTest, AdminEndpoint) {
  TestConnectionOptions conn_opts(grpc::InsecureChannelCredentials());
  EXPECT_EQ(TestTraits::default_endpoint(), conn_opts.endpoint());
  EXPECT_EQ(conn_opts.endpoint(),
            internal::MakeOptions(conn_opts).get<EndpointOption>());

  conn_opts.set_endpoint("invalid-endpoint");
  EXPECT_EQ("invalid-endpoint", conn_opts.endpoint());
  EXPECT_EQ(conn_opts.endpoint(),
            internal::MakeOptions(conn_opts).get<EndpointOption>());
}

TEST(ConnectionOptionsTest, NumChannels) {
  TestConnectionOptions conn_opts(grpc::InsecureChannelCredentials());
  int num_channels = conn_opts.num_channels();
  EXPECT_EQ(TestTraits::default_num_channels(), num_channels);
  EXPECT_EQ(conn_opts.num_channels(),
            internal::MakeOptions(conn_opts).get<GrpcNumChannelsOption>());

  num_channels *= 2;  // ensure we change it from the default value.
  conn_opts.set_num_channels(num_channels);
  EXPECT_EQ(num_channels, conn_opts.num_channels());
  EXPECT_EQ(conn_opts.num_channels(),
            internal::MakeOptions(conn_opts).get<GrpcNumChannelsOption>());
}

TEST(ConnectionOptionsTest, Tracing) {
  TestConnectionOptions conn_opts(grpc::InsecureChannelCredentials());
  conn_opts.enable_tracing("fake-component");
  EXPECT_TRUE(conn_opts.tracing_enabled("fake-component"));

  Options opts = internal::MakeOptions(conn_opts);
  auto components = opts.get<TracingComponentsOption>();
  EXPECT_TRUE(internal::Contains(components, "fake-component"));
  EXPECT_EQ(conn_opts.components(), components);

  conn_opts.disable_tracing("fake-component");
  opts = internal::MakeOptions(conn_opts);
  components = opts.get<TracingComponentsOption>();
  EXPECT_FALSE(conn_opts.tracing_enabled("fake-component"));
  EXPECT_FALSE(internal::Contains(components, "fake-component"));
  EXPECT_EQ(conn_opts.components(), components);
}

TEST(ConnectionOptionsTest, DefaultTracingUnset) {
  testing_util::ScopedEnvironment env("GOOGLE_CLOUD_CPP_ENABLE_TRACING", {});
  TestConnectionOptions conn_opts(grpc::InsecureChannelCredentials());
  EXPECT_FALSE(conn_opts.tracing_enabled("rpc"));

  Options opts = internal::MakeOptions(conn_opts);
  auto const& components = opts.get<TracingComponentsOption>();
  EXPECT_EQ(conn_opts.components(), components);
}

TEST(ConnectionOptionsTest, DefaultTracingSet) {
  testing_util::ScopedEnvironment env("GOOGLE_CLOUD_CPP_ENABLE_TRACING",
                                      "foo,bar,baz");
  TestConnectionOptions conn_opts(grpc::InsecureChannelCredentials());
  EXPECT_FALSE(conn_opts.tracing_enabled("rpc"));
  EXPECT_TRUE(conn_opts.tracing_enabled("foo"));
  EXPECT_TRUE(conn_opts.tracing_enabled("bar"));
  EXPECT_TRUE(conn_opts.tracing_enabled("baz"));
  EXPECT_THAT(internal::MakeOptions(conn_opts).get<TracingComponentsOption>(),
              UnorderedElementsAre("foo", "bar", "baz"));
}

TEST(ConnectionOptionsTest, TracingOptions) {
  testing_util::ScopedEnvironment env("GOOGLE_CLOUD_CPP_TRACING_OPTIONS",
                                      ",single_line_mode=off"
                                      ",use_short_repeated_primitives=off"
                                      ",truncate_string_field_longer_than=32");
  TestConnectionOptions conn_opts(grpc::InsecureChannelCredentials());
  TracingOptions tracing_options = conn_opts.tracing_options();
  EXPECT_FALSE(tracing_options.single_line_mode());
  EXPECT_FALSE(tracing_options.use_short_repeated_primitives());
  EXPECT_EQ(32, tracing_options.truncate_string_field_longer_than());
  EXPECT_EQ(conn_opts.tracing_options(),
            internal::MakeOptions(conn_opts).get<GrpcTracingOptionsOption>());
}

TEST(ConnectionOptionsTest, ChannelPoolName) {
  TestConnectionOptions conn_opts(grpc::InsecureChannelCredentials());
  EXPECT_TRUE(conn_opts.channel_pool_domain().empty());
  EXPECT_FALSE(
      internal::MakeOptions(conn_opts).has<GrpcChannelArgumentsOption>());

  conn_opts.set_channel_pool_domain("test-channel-pool");
  EXPECT_EQ("test-channel-pool", conn_opts.channel_pool_domain());
  auto opts =
      internal::MakeOptions(conn_opts).get<GrpcChannelArgumentsOption>();
  EXPECT_EQ(opts["grpc.channel_pooling_domain"], "test-channel-pool");
}

TEST(ConnectionOptionsTest, UserAgentProducts) {
  TestConnectionOptions conn_opts(grpc::InsecureChannelCredentials());
  EXPECT_EQ(TestTraits::user_agent_prefix(), conn_opts.user_agent_prefix());
  EXPECT_THAT(internal::MakeOptions(conn_opts).get<UserAgentProductsOption>(),
              ElementsAre(conn_opts.user_agent_prefix()));

  conn_opts.add_user_agent_prefix("test-prefix/1.2.3");
  EXPECT_EQ("test-prefix/1.2.3 " + TestTraits::user_agent_prefix(),
            conn_opts.user_agent_prefix());
  EXPECT_THAT(internal::MakeOptions(conn_opts).get<UserAgentProductsOption>(),
              ElementsAre(conn_opts.user_agent_prefix()));
}

TEST(ConnectionOptionsTest, CreateChannelArgumentsDefault) {
  TestConnectionOptions conn_opts(grpc::InsecureChannelCredentials());

  auto actual = conn_opts.CreateChannelArguments();

  auto user_agent = internal::GetStringChannelArgument(actual, "grpc.primary_user_agent");
  ASSERT_TRUE(user_agent.has_value());
  // The gRPC library adds its own version to the user-agent string, so we only
  // check that our component appears in it.
  EXPECT_THAT(*user_agent, StartsWith(conn_opts.user_agent_prefix()));
}

TEST(ConnectionOptionsTest, CreateChannelArgumentsWithChannelPool) {
  TestConnectionOptions conn_opts(grpc::InsecureChannelCredentials());
  conn_opts.set_channel_pool_domain("testing-pool");
  conn_opts.add_user_agent_prefix("test-prefix/1.2.3");

  auto actual = conn_opts.CreateChannelArguments();

  auto testing_pool = internal::GetStringChannelArgument(actual, "grpc.channel_pooling_domain");
  ASSERT_TRUE(testing_pool.has_value());
  EXPECT_THAT(*testing_pool, StartsWith(conn_opts.channel_pool_domain()));

  auto user_agent = internal::GetStringChannelArgument(actual, "grpc.primary_user_agent");
  ASSERT_TRUE(user_agent.has_value());
  // The gRPC library adds its own version to the user-agent string, so we only
  // check that our component appears in it.
  EXPECT_THAT(*user_agent, StartsWith(conn_opts.user_agent_prefix()));
}

TEST(ConnectionOptionsTest, CustomBackgroundThreads) {
  CompletionQueue cq;

  auto conn_opts = TestConnectionOptions(grpc::InsecureChannelCredentials())
                       .DisableBackgroundThreads(cq);
  auto background = conn_opts.background_threads_factory()();

  using ms = std::chrono::milliseconds;

  // Schedule some work, it cannot execute because there is no thread attached.
  promise<std::thread::id> p;
  auto background_thread_id = p.get_future();
  background->cq().RunAsync(
      [&p](CompletionQueue&) { p.set_value(std::this_thread::get_id()); });
  EXPECT_NE(std::future_status::ready, background_thread_id.wait_for(ms(1)));

  // Verify we can create our own threads to drain the completion queue.
  std::thread t([&cq] { cq.Run(); });
  EXPECT_EQ(t.get_id(), background_thread_id.get());

  cq.Shutdown();
  t.join();
}

TEST(ConnectionOptionsTest, DefaultBackgroundThreads) {
  auto constexpr kThreadCount = 4;
  auto conn_opts = TestConnectionOptions(grpc::InsecureChannelCredentials())
                       .set_background_thread_pool_size(kThreadCount);
  auto background = conn_opts.background_threads_factory()();
  auto* tp = dynamic_cast<internal::AutomaticallyCreatedBackgroundThreads*>(
      background.get());
  ASSERT_NE(nullptr, tp);
  EXPECT_EQ(kThreadCount, tp->pool_size());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
