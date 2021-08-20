// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/client_options.h"
#include "google/cloud/bigtable/internal/client_options_defaults.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/types/optional.h"
#include "options.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
struct ClientOptionsTestTraits {
  static std::string const& InstanceAdminEndpoint(
      ClientOptions const& options) {
    return options.instance_admin_endpoint();
  }
};

namespace {
using ::google::cloud::internal::GetIntChannelArgument;
using ::google::cloud::internal::GetStringChannelArgument;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::HasSubstr;
}  // namespace

TEST(ClientOptionsTest, ClientOptionsDefaultSettings) {
  ClientOptions client_options;
  EXPECT_EQ("bigtable.googleapis.com", client_options.data_endpoint());
  EXPECT_EQ("bigtableadmin.googleapis.com", client_options.admin_endpoint());
  EXPECT_EQ(typeid(grpc::GoogleDefaultCredentials()),
            typeid(client_options.credentials()));

  EXPECT_EQ("", client_options.connection_pool_name());
  // The number of connections should be >= 1, we "know" what the actual value
  // is, but we do not want a change-detection-test.
  EXPECT_LE(1UL, client_options.connection_pool_size());

  auto args = client_options.channel_arguments();
  auto max_send = GetIntChannelArgument(args, GRPC_ARG_MAX_SEND_MESSAGE_LENGTH);
  ASSERT_TRUE(max_send.has_value());
  EXPECT_EQ(BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH, max_send.value());
  auto max_recv =
      GetIntChannelArgument(args, GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH);
  ASSERT_TRUE(max_recv.has_value());
  EXPECT_EQ(BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH, max_recv.value());

  // See `kDefaultKeepaliveTime`
  auto time = GetIntChannelArgument(args, GRPC_ARG_KEEPALIVE_TIME_MS);
  ASSERT_TRUE(time.has_value());
  EXPECT_EQ(30000, time.value());

  // See `kDefaultKeepaliveTimeout`
  auto timeout = GetIntChannelArgument(args, GRPC_ARG_KEEPALIVE_TIMEOUT_MS);
  ASSERT_TRUE(timeout.has_value());
  EXPECT_EQ(10000, timeout.value());
}

TEST(ClientOptionsTest, OptionsConstructor) {
  using ms = std::chrono::milliseconds;
  using min = std::chrono::minutes;
  auto credentials = grpc::InsecureChannelCredentials();
  auto options = ClientOptions(
      Options{}
          .set<DataEndpointOption>("testdata.googleapis.com")
          .set<AdminEndpointOption>("testadmin.googleapis.com")
          .set<InstanceAdminEndpointOption>("testinstanceadmin.googleapis.com")
          .set<GrpcCredentialOption>(credentials)
          .set<GrpcTracingOptionsOption>(
              TracingOptions{}.SetOptions("single_line_mode=F"))
          .set<TracingComponentsOption>({"test-component"})
          .set<GrpcNumChannelsOption>(3)
          .set<MinConnectionRefreshOption>(ms(100))
          .set<MaxConnectionRefreshOption>(min(4))
          .set<GrpcBackgroundThreadPoolSizeOption>(5));

  EXPECT_EQ("testdata.googleapis.com", options.data_endpoint());
  EXPECT_EQ("testadmin.googleapis.com", options.admin_endpoint());
  EXPECT_EQ("testinstanceadmin.googleapis.com",
            ClientOptionsTestTraits::InstanceAdminEndpoint(options));
  EXPECT_EQ(credentials, options.credentials());
  EXPECT_FALSE(options.tracing_options().single_line_mode());
  EXPECT_TRUE(options.tracing_enabled("test-component"));
  EXPECT_EQ(3U, options.connection_pool_size());
  EXPECT_EQ(ms(100), options.min_conn_refresh_period());
  EXPECT_EQ(min(4), options.max_conn_refresh_period());
  EXPECT_EQ(5U, options.background_thread_pool_size());
}

TEST(ClientOptionsTest, CustomBackgroundThreadsOption) {
  struct Fake : BackgroundThreads {
    CompletionQueue cq() const override { return {}; }
  };
  bool invoked = false;
  auto factory = [&invoked] {
    invoked = true;
    return absl::make_unique<Fake>();
  };

  auto options =
      ClientOptions(Options{}.set<GrpcBackgroundThreadsFactoryOption>(factory));

  EXPECT_FALSE(invoked);
  options.background_threads_factory()();
  EXPECT_TRUE(invoked);
}

class ClientOptionsDefaultEndpointTest : public ::testing::Test {
 public:
  ClientOptionsDefaultEndpointTest()
      : bigtable_emulator_host_("BIGTABLE_EMULATOR_HOST",
                                "testendpoint.googleapis.com"),
        bigtable_instance_admin_emulator_host_(
            "BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST", {}) {}

 protected:
  static std::string GetInstanceAdminEndpoint(ClientOptions const& options) {
    return ClientOptionsTestTraits::InstanceAdminEndpoint(options);
  }

  ScopedEnvironment bigtable_emulator_host_;
  ScopedEnvironment bigtable_instance_admin_emulator_host_;
};

TEST_F(ClientOptionsDefaultEndpointTest, Default) {
  ClientOptions client_options;
  EXPECT_EQ("testendpoint.googleapis.com", client_options.data_endpoint());
  EXPECT_EQ("testendpoint.googleapis.com", client_options.admin_endpoint());
  EXPECT_EQ("testendpoint.googleapis.com",
            GetInstanceAdminEndpoint(client_options));
}

TEST_F(ClientOptionsDefaultEndpointTest, WithCredentials) {
  auto credentials = grpc::GoogleDefaultCredentials();
  ClientOptions tested(credentials);
  EXPECT_EQ("bigtable.googleapis.com", tested.data_endpoint());
  EXPECT_EQ("bigtableadmin.googleapis.com", tested.admin_endpoint());
  EXPECT_EQ(credentials.get(), tested.credentials().get());
}

TEST_F(ClientOptionsDefaultEndpointTest, DefaultNoEmulator) {
  ScopedEnvironment bigtable_emulator_host("BIGTABLE_EMULATOR_HOST", {});

  auto credentials = grpc::GoogleDefaultCredentials();
  ClientOptions tested(credentials);
  EXPECT_EQ("bigtable.googleapis.com", tested.data_endpoint());
  EXPECT_EQ("bigtableadmin.googleapis.com", tested.admin_endpoint());
  EXPECT_EQ("bigtableadmin.googleapis.com", GetInstanceAdminEndpoint(tested));
}

TEST_F(ClientOptionsDefaultEndpointTest, SeparateEmulators) {
  ScopedEnvironment bigtable_emulator_host("BIGTABLE_EMULATOR_HOST",
                                           "emulator-host:8000");
  ScopedEnvironment bigtable_instance_admin_emulator_host(
      "BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST", "instance-emulator-host:9000");
  ClientOptions actual;
  EXPECT_EQ("emulator-host:8000", actual.data_endpoint());
  EXPECT_EQ("emulator-host:8000", actual.admin_endpoint());
  EXPECT_EQ("instance-emulator-host:9000", GetInstanceAdminEndpoint(actual));
}

TEST_F(ClientOptionsDefaultEndpointTest, DataNoEnv) {
  ScopedEnvironment bigtable_emulator_host("BIGTABLE_EMULATOR_HOST", {});
  EXPECT_EQ("bigtable.googleapis.com", ClientOptions{}.data_endpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, AdminNoEnv) {
  ScopedEnvironment bigtable_emulator_host("BIGTABLE_EMULATOR_HOST", {});
  EXPECT_EQ("bigtableadmin.googleapis.com", ClientOptions{}.admin_endpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, AdminEmulatorOverrides) {
  ScopedEnvironment bigtable_emulator_host("BIGTABLE_EMULATOR_HOST",
                                           "127.0.0.1:1234");
  EXPECT_EQ("127.0.0.1:1234", ClientOptions{}.admin_endpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, AdminInstanceAdminNoEffect) {
  ScopedEnvironment bigtable_emulator_host("BIGTABLE_EMULATOR_HOST", {});
  ScopedEnvironment bigtable_instance_admin_emulator_host(
      "BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST", "127.0.0.1:1234");
  EXPECT_EQ("bigtableadmin.googleapis.com", ClientOptions{}.admin_endpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, InstanceAdminNoEnv) {
  ScopedEnvironment bigtable_emulator_host("BIGTABLE_EMULATOR_HOST", {});
  EXPECT_EQ("bigtableadmin.googleapis.com",
            GetInstanceAdminEndpoint(ClientOptions()));
}

TEST_F(ClientOptionsDefaultEndpointTest, InstanceAdminEmulatorOverrides) {
  ScopedEnvironment bigtable_emulator_host("BIGTABLE_EMULATOR_HOST",
                                           "127.0.0.1:1234");
  EXPECT_EQ("127.0.0.1:1234", GetInstanceAdminEndpoint(ClientOptions()));
}

TEST_F(ClientOptionsDefaultEndpointTest, InstanceAdminInstanceAdminOverrides) {
  ScopedEnvironment bigtable_emulator_host("BIGTABLE_EMULATOR_HOST", "unused");
  ScopedEnvironment bigtable_instance_admin_emulator_host(
      "BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST", "127.0.0.1:1234");
  EXPECT_EQ("127.0.0.1:1234", GetInstanceAdminEndpoint(ClientOptions()));
}

TEST(ClientOptionsTest, EditDataEndpoint) {
  auto client_options = ClientOptions{}.set_data_endpoint("customendpoint.com");
  EXPECT_EQ("customendpoint.com", client_options.data_endpoint());
}

TEST(ClientOptionsTest, EditAdminEndpoint) {
  auto client_options =
      ClientOptions{}.set_admin_endpoint("customendpoint.com");
  EXPECT_EQ("customendpoint.com", client_options.admin_endpoint());
  EXPECT_EQ("customendpoint.com",
            ClientOptionsTestTraits::InstanceAdminEndpoint(client_options));
}

TEST(ClientOptionsTest, EditCredentials) {
  auto client_options =
      ClientOptions{}.SetCredentials(grpc::InsecureChannelCredentials());
  EXPECT_EQ(typeid(grpc::InsecureChannelCredentials()),
            typeid(client_options.credentials()));
}

TEST(ClientOptionsTest, EditConnectionPoolName) {
  ClientOptions client_options;
  auto& returned = client_options.set_connection_pool_name("foo");
  EXPECT_EQ(&returned, &client_options);
  EXPECT_EQ("foo", returned.connection_pool_name());
}

TEST(ClientOptionsTest, EditConnectionPoolSize) {
  ClientOptions client_options;
  auto& returned = client_options.set_connection_pool_size(42);
  EXPECT_EQ(&returned, &client_options);
  EXPECT_EQ(42UL, returned.connection_pool_size());
}

TEST(ClientOptionsTest, ResetToDefaultConnectionPoolSize) {
  ClientOptions client_options;
  auto& returned = client_options.set_connection_pool_size(0);
  EXPECT_EQ(&returned, &client_options);
  // The number of connections should be >= 1, we "know" what the actual value
  // is, but we do not want a change-detection-test.
  EXPECT_LE(1UL, returned.connection_pool_size());
}

TEST(ClientOptionsTest, ConnectionPoolSizeDoesNotExceedMax) {
  ClientOptions client_options;
  auto& returned = client_options.set_connection_pool_size(
      BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE_MAX + 1);
  EXPECT_EQ(&returned, &client_options);
  // The number of connections should be >= 1, we "know" what the actual value
  // is, but we do not want a change-detection-test.
  EXPECT_LE(BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE_MAX,
            returned.connection_pool_size());
}

TEST(ClientOptionsTest, SetGrpclbFallbackTimeoutMS) {
  // Test milliseconds are set properly to channel_arguments
  ClientOptions client_options;
  ASSERT_STATUS_OK(
      client_options.SetGrpclbFallbackTimeout(std::chrono::milliseconds(5)));
  auto const actual = GetIntChannelArgument(
      client_options.channel_arguments(), GRPC_ARG_GRPCLB_FALLBACK_TIMEOUT_MS);
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(*actual, 5);
}

TEST(ClientOptionsTest, SetGrpclbFallbackTimeoutSec) {
  // Test seconds are converted into milliseconds
  ClientOptions client_options;
  ASSERT_STATUS_OK(
      client_options.SetGrpclbFallbackTimeout(std::chrono::seconds(5)));
  auto const actual = GetIntChannelArgument(
      client_options.channel_arguments(), GRPC_ARG_GRPCLB_FALLBACK_TIMEOUT_MS);
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(*actual, 5000);
}

TEST(ClientOptionsTest, SetGrpclbFallbackTimeoutException) {
  // Test if fallback_timeout exceeds int range and StatusCode
  // matches kOutOfRange
  ClientOptions client_options;
  EXPECT_EQ(
      client_options.SetGrpclbFallbackTimeout(std::chrono::hours(999)).code(),
      google::cloud::StatusCode::kOutOfRange);
}

TEST(ClientOptionsTest, SetCompressionAlgorithm) {
  ClientOptions client_options;
  client_options.SetCompressionAlgorithm(GRPC_COMPRESS_NONE);
  auto const actual =
      GetIntChannelArgument(client_options.channel_arguments(),
                            GRPC_COMPRESSION_CHANNEL_DEFAULT_ALGORITHM);
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(*actual, GRPC_COMPRESS_NONE);
}

TEST(ClientOptionsTest, SetLoadBalancingPolicyName) {
  ClientOptions client_options;
  client_options.SetLoadBalancingPolicyName("test-policy-name");
  auto const actual = GetStringChannelArgument(
      client_options.channel_arguments(), GRPC_ARG_LB_POLICY_NAME);
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(*actual, "test-policy-name");
}

TEST(ClientOptionsTest, SetServiceConfigJSON) {
  ClientOptions client_options;
  client_options.SetServiceConfigJSON("test-config");
  auto const actual = GetStringChannelArgument(
      client_options.channel_arguments(), GRPC_ARG_SERVICE_CONFIG);
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(*actual, "test-config");
}

TEST(ClientOptionsTest, SetUserAgentPrefix) {
  ClientOptions client_options;
  client_options.SetUserAgentPrefix("test_prefix");
  auto const actual = GetStringChannelArgument(
      client_options.channel_arguments(), GRPC_ARG_PRIMARY_USER_AGENT_STRING);
  ASSERT_TRUE(actual.has_value());
  EXPECT_THAT(*actual, HasSubstr("test_prefix"));
}

TEST(ClientOptionsTest, SetSslTargetNameOverride) {
  ClientOptions client_options;
  client_options.SetSslTargetNameOverride("test-name");
  auto const actual = GetStringChannelArgument(
      client_options.channel_arguments(), GRPC_SSL_TARGET_NAME_OVERRIDE_ARG);
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(*actual, "test-name");
}

TEST(ClientOptionsTest, UserAgentPrefix) {
  std::string const actual = ClientOptions::UserAgentPrefix();
  EXPECT_THAT(actual, HasSubstr("gcloud-cpp/"));
}

TEST(ClientOptionsTest, RefreshPeriod) {
  auto options = ClientOptions();
  EXPECT_LE(options.min_conn_refresh_period(),
            options.max_conn_refresh_period());
  using ms = std::chrono::milliseconds;

  options.set_min_conn_refresh_period(ms(1000));
  EXPECT_EQ(1000, options.min_conn_refresh_period().count());

  options.set_max_conn_refresh_period(ms(2000));
  EXPECT_EQ(2000, options.max_conn_refresh_period().count());

  options.set_min_conn_refresh_period(ms(3000));
  EXPECT_EQ(3000, options.min_conn_refresh_period().count());
  EXPECT_EQ(3000, options.max_conn_refresh_period().count());

  options.set_max_conn_refresh_period(ms(1500));
  EXPECT_EQ(1500, options.min_conn_refresh_period().count());
  EXPECT_EQ(1500, options.max_conn_refresh_period().count());

  options.set_max_conn_refresh_period(ms(5000));
  EXPECT_EQ(1500, options.min_conn_refresh_period().count());
  EXPECT_EQ(5000, options.max_conn_refresh_period().count());

  options.set_min_conn_refresh_period(ms(1000));
  EXPECT_EQ(1000, options.min_conn_refresh_period().count());
  EXPECT_EQ(5000, options.max_conn_refresh_period().count());
}

TEST(ClientOptionsTest, TracingComponents) {
  ScopedEnvironment tracing("GOOGLE_CLOUD_CPP_ENABLE_TRACING", "foo,bar");
  auto options = ClientOptions();

  // Check defaults
  EXPECT_TRUE(options.tracing_enabled("foo"));
  EXPECT_TRUE(options.tracing_enabled("bar"));
  EXPECT_FALSE(options.tracing_enabled("baz"));

  // Edit components
  options.enable_tracing("baz");
  EXPECT_TRUE(options.tracing_enabled("baz"));
  options.disable_tracing("foo");
  EXPECT_FALSE(options.tracing_enabled("foo"));
}

TEST(ClientOptionsTest, DefaultTracingOptionsNoEnv) {
  ScopedEnvironment tracing("GOOGLE_CLOUD_CPP_TRACING_OPTIONS", absl::nullopt);
  auto options = ClientOptions().tracing_options();
  EXPECT_EQ(TracingOptions(), options);
}

TEST(ClientOptionsTest, DefaultTracingOptionsEnv) {
  ScopedEnvironment tracing("GOOGLE_CLOUD_CPP_TRACING_OPTIONS",
                            "single_line_mode=F");
  auto options = ClientOptions().tracing_options();
  EXPECT_FALSE(options.single_line_mode());
}

TEST(ClientOptionsTest, BackgroundThreadPoolSize) {
  using ThreadPool =
      ::google::cloud::internal::AutomaticallyCreatedBackgroundThreads;

  auto options = ClientOptions();
  // Check that the default value is 0 or 1. Both values result in the
  // BackgroundThreadsFactory creating a ThreadPool with a single thread in it.
  EXPECT_GE(1U, options.background_thread_pool_size());

  auto background = options.background_threads_factory()();
  auto* tp = dynamic_cast<ThreadPool*>(background.get());
  ASSERT_NE(nullptr, tp);
  EXPECT_EQ(1U, tp->pool_size());

  options.set_background_thread_pool_size(5U);
  EXPECT_EQ(5U, options.background_thread_pool_size());

  background = options.background_threads_factory()();
  tp = dynamic_cast<ThreadPool*>(background.get());
  ASSERT_NE(nullptr, tp);
  EXPECT_EQ(5U, tp->pool_size());
}

TEST(ClientOptionsTest, CustomBackgroundThreads) {
  CompletionQueue cq;
  auto options = ClientOptions().DisableBackgroundThreads(cq);
  auto background = options.background_threads_factory()();

  using ms = std::chrono::milliseconds;

  // Schedule some work that cannot execute because there is no thread draining
  // the completion queue.
  promise<std::thread::id> p;
  auto background_thread_id = p.get_future();
  background->cq().RunAsync(
      [&p](CompletionQueue&) { p.set_value(std::this_thread::get_id()); });
  EXPECT_NE(std::future_status::ready, background_thread_id.wait_for(ms(10)));

  // Verify we can create our own threads to drain the completion queue.
  std::thread t([&cq] { cq.Run(); });
  EXPECT_EQ(t.get_id(), background_thread_id.get());

  cq.Shutdown();
  t.join();
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
