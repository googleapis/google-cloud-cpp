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
#include "google/cloud/status.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/types/optional.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
struct ClientOptionsTestTraits {
  static std::string const& InstanceAdminEndpoint(
      bigtable::ClientOptions const& options) {
    return options.instance_admin_endpoint();
  }
};

namespace {

using ::testing::HasSubstr;

absl::optional<int> GetInt(grpc::ChannelArguments const& args,
                           std::string const& key) {
  auto c_args = args.c_channel_args();
  // Just do a linear search for the key; the data structure is not organized
  // in any other useful way.
  for (auto const* a = c_args.args; a != c_args.args + c_args.num_args; ++a) {
    if (key != a->key) continue;
    if (a->type != GRPC_ARG_INTEGER) return absl::nullopt;
    return a->value.integer;
  }
  return absl::nullopt;
}

absl::optional<std::string> GetString(grpc::ChannelArguments const& args,
                                      std::string const& key) {
  auto c_args = args.c_channel_args();
  // Just do a linear search for the key; the data structure is not organized
  // in any other useful way.
  for (auto const* a = c_args.args; a != c_args.args + c_args.num_args; ++a) {
    if (key != a->key) continue;
    if (a->type != GRPC_ARG_STRING) return absl::nullopt;
    return a->value.string;
  }
  return absl::nullopt;
}

TEST(ClientOptionsTest, ClientOptionsDefaultSettings) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  EXPECT_EQ("bigtable.googleapis.com", client_options_object.data_endpoint());
  EXPECT_EQ("bigtableadmin.googleapis.com",
            client_options_object.admin_endpoint());
  EXPECT_EQ(typeid(grpc::GoogleDefaultCredentials()),
            typeid(client_options_object.credentials()));

  EXPECT_EQ("", client_options_object.connection_pool_name());
  // The number of connections should be >= 1, we "know" what the actual value
  // is, but we do not want a change-detection-test.
  EXPECT_LE(1UL, client_options_object.connection_pool_size());
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

  google::cloud::testing_util::ScopedEnvironment bigtable_emulator_host_;
  google::cloud::testing_util::ScopedEnvironment
      bigtable_instance_admin_emulator_host_;
};

TEST_F(ClientOptionsDefaultEndpointTest, Default) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  EXPECT_EQ("testendpoint.googleapis.com",
            client_options_object.data_endpoint());
  EXPECT_EQ("testendpoint.googleapis.com",
            client_options_object.admin_endpoint());
  EXPECT_EQ("testendpoint.googleapis.com",
            GetInstanceAdminEndpoint(client_options_object));
}

TEST_F(ClientOptionsDefaultEndpointTest, WithCredentials) {
  auto credentials = grpc::GoogleDefaultCredentials();
  bigtable::ClientOptions tested(credentials);
  EXPECT_EQ("bigtable.googleapis.com", tested.data_endpoint());
  EXPECT_EQ("bigtableadmin.googleapis.com", tested.admin_endpoint());
  EXPECT_EQ(credentials.get(), tested.credentials().get());
}

TEST_F(ClientOptionsDefaultEndpointTest, DefaultNoEmulator) {
  google::cloud::testing_util::ScopedEnvironment bigtable_emulator_host(
      "BIGTABLE_EMULATOR_HOST", {});

  auto credentials = grpc::GoogleDefaultCredentials();
  bigtable::ClientOptions tested(credentials);
  EXPECT_EQ("bigtable.googleapis.com", tested.data_endpoint());
  EXPECT_EQ("bigtableadmin.googleapis.com", tested.admin_endpoint());
  EXPECT_EQ("bigtableadmin.googleapis.com", GetInstanceAdminEndpoint(tested));
}

TEST_F(ClientOptionsDefaultEndpointTest, SeparateEmulators) {
  google::cloud::testing_util::ScopedEnvironment bigtable_emulator_host(
      "BIGTABLE_EMULATOR_HOST", "emulator-host:8000");
  google::cloud::testing_util::ScopedEnvironment
      bigtable_instance_admin_emulator_host(
          "BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST",
          "instance-emulator-host:9000");
  bigtable::ClientOptions actual = bigtable::ClientOptions();
  EXPECT_EQ("emulator-host:8000", actual.data_endpoint());
  EXPECT_EQ("emulator-host:8000", actual.admin_endpoint());
  EXPECT_EQ("instance-emulator-host:9000", GetInstanceAdminEndpoint(actual));
}

TEST_F(ClientOptionsDefaultEndpointTest, DataNoEnv) {
  google::cloud::testing_util::ScopedEnvironment bigtable_emulator_host(
      "BIGTABLE_EMULATOR_HOST", {});

  EXPECT_EQ("bigtable.googleapis.com", internal::DefaultDataEndpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, AdminNoEnv) {
  google::cloud::testing_util::ScopedEnvironment bigtable_emulator_host(
      "BIGTABLE_EMULATOR_HOST", {});

  EXPECT_EQ("bigtableadmin.googleapis.com", internal::DefaultAdminEndpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, AdminEmulatorOverrides) {
  google::cloud::testing_util::ScopedEnvironment bigtable_emulator_host(
      "BIGTABLE_EMULATOR_HOST", "127.0.0.1:1234");

  EXPECT_EQ("127.0.0.1:1234", internal::DefaultAdminEndpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, AdminInstanceAdminNoEffect) {
  google::cloud::testing_util::ScopedEnvironment bigtable_emulator_host(
      "BIGTABLE_EMULATOR_HOST", {});
  google::cloud::testing_util::ScopedEnvironment
      bigtable_instance_admin_emulator_host(
          "BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST", "127.0.0.1:1234");

  EXPECT_EQ("bigtableadmin.googleapis.com", internal::DefaultAdminEndpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, InstanceAdminNoEnv) {
  google::cloud::testing_util::ScopedEnvironment bigtable_emulator_host(
      "BIGTABLE_EMULATOR_HOST", {});

  EXPECT_EQ("bigtableadmin.googleapis.com",
            internal::DefaultInstanceAdminEndpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, InstanceAdminEmulatorOverrides) {
  google::cloud::testing_util::ScopedEnvironment bigtable_emulator_host(
      "BIGTABLE_EMULATOR_HOST", "127.0.0.1:1234");

  EXPECT_EQ("127.0.0.1:1234", internal::DefaultInstanceAdminEndpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, InstanceAdminInstanceAdminOverrides) {
  google::cloud::testing_util::ScopedEnvironment bigtable_emulator_host(
      "BIGTABLE_EMULATOR_HOST", "unused");
  google::cloud::testing_util::ScopedEnvironment
      bigtable_instance_admin_emulator_host(
          "BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST", "127.0.0.1:1234");

  EXPECT_EQ("127.0.0.1:1234", internal::DefaultInstanceAdminEndpoint());
}

TEST(ClientOptionsTest, EditDataEndpoint) {
  bigtable::ClientOptions client_options_object;
  client_options_object =
      client_options_object.set_data_endpoint("customendpoint.com");
  EXPECT_EQ("customendpoint.com", client_options_object.data_endpoint());
}

TEST(ClientOptionsTest, EditAdminEndpoint) {
  bigtable::ClientOptions client_options_object;
  client_options_object =
      client_options_object.set_admin_endpoint("customendpoint.com");
  EXPECT_EQ("customendpoint.com", client_options_object.admin_endpoint());
  EXPECT_EQ(
      "customendpoint.com",
      ClientOptionsTestTraits::InstanceAdminEndpoint(client_options_object));
}

TEST(ClientOptionsTest, EditCredentials) {
  bigtable::ClientOptions client_options_object;
  client_options_object =
      client_options_object.SetCredentials(grpc::InsecureChannelCredentials());
  EXPECT_EQ(typeid(grpc::InsecureChannelCredentials()),
            typeid(client_options_object.credentials()));
}

TEST(ClientOptionsTest, EditConnectionPoolName) {
  bigtable::ClientOptions client_options_object;
  auto& returned = client_options_object.set_connection_pool_name("foo");
  EXPECT_EQ(&returned, &client_options_object);
  EXPECT_EQ("foo", returned.connection_pool_name());
}

TEST(ClientOptionsTest, EditConnectionPoolSize) {
  bigtable::ClientOptions client_options_object;
  auto& returned = client_options_object.set_connection_pool_size(42);
  EXPECT_EQ(&returned, &client_options_object);
  EXPECT_EQ(42UL, returned.connection_pool_size());
}

TEST(ClientOptionsTest, ResetToDefaultConnectionPoolSize) {
  bigtable::ClientOptions client_options_object;
  auto& returned = client_options_object.set_connection_pool_size(0);
  EXPECT_EQ(&returned, &client_options_object);
  // The number of connections should be >= 1, we "know" what the actual value
  // is, but we do not want a change-detection-test.
  EXPECT_LE(1UL, returned.connection_pool_size());
}

TEST(ClientOptionsTest, ConnectionPoolSizeDoesNotExceedMax) {
  bigtable::ClientOptions client_options_object;
  auto& returned = client_options_object.set_connection_pool_size(
      BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE_MAX + 1);
  EXPECT_EQ(&returned, &client_options_object);
  // The number of connections should be >= 1, we "know" what the actual value
  // is, but we do not want a change-detection-test.
  EXPECT_LE(BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE_MAX,
            returned.connection_pool_size());
}

TEST(ClientOptionsTest, SetGrpclbFallbackTimeoutMS) {
  // Test milliseconds are set properly to channel_arguments
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  ASSERT_STATUS_OK(client_options_object.SetGrpclbFallbackTimeout(
      std::chrono::milliseconds(5)));
  auto const actual = GetInt(client_options_object.channel_arguments(),
                             GRPC_ARG_GRPCLB_FALLBACK_TIMEOUT_MS);
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(*actual, 5);
}

TEST(ClientOptionsTest, SetGrpclbFallbackTimeoutSec) {
  // Test seconds are converted into milliseconds
  bigtable::ClientOptions client_options_object_second =
      bigtable::ClientOptions();
  ASSERT_STATUS_OK(client_options_object_second.SetGrpclbFallbackTimeout(
      std::chrono::seconds(5)));
  auto const actual = GetInt(client_options_object_second.channel_arguments(),
                             GRPC_ARG_GRPCLB_FALLBACK_TIMEOUT_MS);
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(*actual, 5000);
}

TEST(ClientOptionsTest, SetGrpclbFallbackTimeoutException) {
  // Test if fallback_timeout exceeds int range and StatusCode
  // matches kOutOfRange
  bigtable::ClientOptions client_options_object_third =
      bigtable::ClientOptions();
  EXPECT_EQ(client_options_object_third
                .SetGrpclbFallbackTimeout(std::chrono::hours(999))
                .code(),
            google::cloud::StatusCode::kOutOfRange);
}

TEST(ClientOptionsTest, SetCompressionAlgorithm) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetCompressionAlgorithm(GRPC_COMPRESS_NONE);
  auto const actual = GetInt(client_options_object.channel_arguments(),
                             GRPC_COMPRESSION_CHANNEL_DEFAULT_ALGORITHM);
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(*actual, GRPC_COMPRESS_NONE);
}

TEST(ClientOptionsTest, SetMaxReceiveMessageSize) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  auto constexpr kExpected = 256 * 1024L * 1024L;
  client_options_object.SetMaxReceiveMessageSize(kExpected);
  auto const actual = GetInt(client_options_object.channel_arguments(),
                             GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH);
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(*actual, kExpected);
}

TEST(ClientOptionsTest, SetMaxSendMessageSize) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  auto constexpr kExpected = 256 * 1024L * 1024L;
  client_options_object.SetMaxSendMessageSize(kExpected);
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  auto const actual = GetInt(client_options_object.channel_arguments(),
                             GRPC_ARG_MAX_SEND_MESSAGE_LENGTH);
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(*actual, kExpected);
}

TEST(ClientOptionsTest, SetLoadBalancingPolicyName) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetLoadBalancingPolicyName("test-policy-name");
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  auto const actual = GetString(client_options_object.channel_arguments(),
                                GRPC_ARG_LB_POLICY_NAME);
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(*actual, "test-policy-name");
}

TEST(ClientOptionsTest, SetServiceConfigJSON) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetServiceConfigJSON("test-config");
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  auto const actual = GetString(client_options_object.channel_arguments(),
                                GRPC_ARG_SERVICE_CONFIG);
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(*actual, "test-config");
}

TEST(ClientOptionsTest, SetUserAgentPrefix) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetUserAgentPrefix("test_prefix");
  auto const actual = GetString(client_options_object.channel_arguments(),
                                GRPC_ARG_PRIMARY_USER_AGENT_STRING);
  ASSERT_TRUE(actual.has_value());
  EXPECT_THAT(*actual, HasSubstr("test_prefix"));
}

TEST(ClientOptionsTest, SetSslTargetNameOverride) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetSslTargetNameOverride("test-name");
  auto const actual = GetString(client_options_object.channel_arguments(),
                                GRPC_SSL_TARGET_NAME_OVERRIDE_ARG);
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(*actual, "test-name");
}

TEST(ClientOptionsTest, UserAgentPrefix) {
  std::string const actual = bigtable::ClientOptions::UserAgentPrefix();

  EXPECT_THAT(actual, HasSubstr("gcloud-cpp/"));
}

TEST(ClientOptionsTest, RefreshPeriod) {
  auto options = bigtable::ClientOptions();
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

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
