// Copyright 2020 Google LLC
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

#include "google/cloud/connection_options.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/compiler_info.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>
#include <map>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {

using ::testing::ElementsAre;
using ::testing::StartsWith;

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
              testing::UnorderedElementsAre("foo", "bar", "baz"));
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
              testing::ElementsAre(conn_opts.user_agent_prefix()));

  conn_opts.add_user_agent_prefix("test-prefix/1.2.3");
  EXPECT_EQ("test-prefix/1.2.3 " + TestTraits::user_agent_prefix(),
            conn_opts.user_agent_prefix());
  EXPECT_THAT(internal::MakeOptions(conn_opts).get<UserAgentProductsOption>(),
              testing::ElementsAre(conn_opts.user_agent_prefix()));
}

TEST(ConnectionOptionsTest, CreateChannelArgumentsDefault) {
  TestConnectionOptions conn_opts(grpc::InsecureChannelCredentials());

  auto actual = conn_opts.CreateChannelArguments();

  // Use the low-level C API because grpc::ChannelArguments lacks high-level
  // accessors.
  grpc_channel_args test_args = actual.c_channel_args();
  ASSERT_EQ(1, test_args.num_args);
  ASSERT_EQ(GRPC_ARG_STRING, test_args.args[0].type);
  EXPECT_EQ("grpc.primary_user_agent", std::string(test_args.args[0].key));
  // The gRPC library adds its own version to the user-agent string, so we only
  // check that our component appears in it.
  EXPECT_THAT(std::string(test_args.args[0].value.string),
              StartsWith(conn_opts.user_agent_prefix()));
}

TEST(ConnectionOptionsTest, CreateChannelArgumentsWithChannelPool) {
  TestConnectionOptions conn_opts(grpc::InsecureChannelCredentials());
  conn_opts.set_channel_pool_domain("testing-pool");
  conn_opts.add_user_agent_prefix("test-prefix/1.2.3");

  auto actual = conn_opts.CreateChannelArguments();

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
              StartsWith(conn_opts.user_agent_prefix()));
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

TEST(ConnectionOptionsTest, DefaultTracingComponentsNoEnvironment) {
  testing_util::ScopedEnvironment env("GOOGLE_CLOUD_CPP_ENABLE_TRACING", {});
  auto const actual = internal::DefaultTracingComponents();
  EXPECT_THAT(actual, ElementsAre());
}

TEST(ConnectionOptionsTest, DefaultTracingComponentsWithValue) {
  testing_util::ScopedEnvironment env("GOOGLE_CLOUD_CPP_ENABLE_TRACING",
                                      "a,b,c");
  auto const actual = internal::DefaultTracingComponents();
  EXPECT_THAT(actual, ElementsAre("a", "b", "c"));
}

TEST(ConnectionOptionsTest, DefaultTracingOptionsNoEnvironment) {
  testing_util::ScopedEnvironment env("GOOGLE_CLOUD_CPP_TRACING_OPTIONS", {});
  auto const actual = internal::DefaultTracingOptions();
  auto const expected = TracingOptions{};
  EXPECT_EQ(expected.single_line_mode(), actual.single_line_mode());
  EXPECT_EQ(expected.use_short_repeated_primitives(),
            actual.use_short_repeated_primitives());
  EXPECT_EQ(expected.truncate_string_field_longer_than(),
            actual.truncate_string_field_longer_than());
}

TEST(ConnectionOptionsTest, DefaultTracingOptionsWithValue) {
  testing_util::ScopedEnvironment env("GOOGLE_CLOUD_CPP_TRACING_OPTIONS",
                                      "single_line_mode=on"
                                      ",use_short_repeated_primitives=ON"
                                      ",truncate_string_field_longer_than=42");
  auto const actual = internal::DefaultTracingOptions();
  EXPECT_TRUE(actual.single_line_mode());
  EXPECT_TRUE(actual.use_short_repeated_primitives());
  EXPECT_EQ(42, actual.truncate_string_field_longer_than());
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
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
