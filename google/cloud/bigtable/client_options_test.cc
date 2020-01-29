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
#include "google/cloud/internal/setenv.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/environment_variable_restore.h"
#include <gmock/gmock.h>
#include <cstdlib>

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
      : bigtable_emulator_host_("BIGTABLE_EMULATOR_HOST"),
        bigtable_instance_admin_emulator_host_(
            "BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST_"),
        google_cloud_enable_direct_path_("GOOGLE_CLOUD_ENABLE_DIRECT_PATH") {}

 protected:
  void SetUp() override {
    bigtable_emulator_host_.SetUp();
    bigtable_instance_admin_emulator_host_.SetUp();
    google_cloud_enable_direct_path_.SetUp();
    google::cloud::internal::SetEnv("BIGTABLE_EMULATOR_HOST",
                                    "testendpoint.googleapis.com");
  }
  void TearDown() override {
    bigtable_emulator_host_.TearDown();
    bigtable_instance_admin_emulator_host_.TearDown();
  }

  std::string GetInstanceAdminEndpoint(ClientOptions const& options) {
    return ClientOptionsTestTraits::InstanceAdminEndpoint(options);
  }

 protected:
  google::cloud::testing_util::EnvironmentVariableRestore
      bigtable_emulator_host_;
  google::cloud::testing_util::EnvironmentVariableRestore
      bigtable_instance_admin_emulator_host_;
  google::cloud::testing_util::EnvironmentVariableRestore
      google_cloud_enable_direct_path_;
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
  // Change the environment variable, TearDown() will restore it.
  google::cloud::internal::UnsetEnv("BIGTABLE_EMULATOR_HOST");
  google::cloud::internal::UnsetEnv("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST");
  auto credentials = grpc::GoogleDefaultCredentials();
  bigtable::ClientOptions tested(credentials);
  EXPECT_EQ("bigtable.googleapis.com", tested.data_endpoint());
  EXPECT_EQ("bigtableadmin.googleapis.com", tested.admin_endpoint());
  EXPECT_EQ("bigtableadmin.googleapis.com", GetInstanceAdminEndpoint(tested));
}

TEST_F(ClientOptionsDefaultEndpointTest, SeparateEmulators) {
  // Change the environment variables, TearDown() will restore them.
  google::cloud::internal::SetEnv("BIGTABLE_EMULATOR_HOST",
                                  "emulator-host:8000");
  google::cloud::internal::SetEnv("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST",
                                  "instance-emulator-host:9000");
  bigtable::ClientOptions actual = bigtable::ClientOptions();
  EXPECT_EQ("emulator-host:8000", actual.data_endpoint());
  EXPECT_EQ("emulator-host:8000", actual.admin_endpoint());
  EXPECT_EQ("instance-emulator-host:9000", GetInstanceAdminEndpoint(actual));
}

TEST_F(ClientOptionsDefaultEndpointTest, DataNoEnv) {
  google::cloud::internal::UnsetEnv("BIGTABLE_EMULATOR_HOST");
  google::cloud::internal::UnsetEnv("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST");
  google::cloud::internal::UnsetEnv("GOOGLE_CLOUD_ENABLE_DIRECT_PATH");

  EXPECT_EQ("bigtable.googleapis.com", internal::DefaultDataEndpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, DataDirectPathMismatched) {
  google::cloud::internal::UnsetEnv("BIGTABLE_EMULATOR_HOST");
  google::cloud::internal::UnsetEnv("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST");
  google::cloud::internal::SetEnv("GOOGLE_CLOUD_ENABLE_DIRECT_PATH",
                                  "bad-name-for-bigtable,bigtable-no-no,bar");

  EXPECT_EQ("bigtable.googleapis.com", internal::DefaultDataEndpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, DataDirectPath) {
  google::cloud::internal::UnsetEnv("BIGTABLE_EMULATOR_HOST");
  google::cloud::internal::UnsetEnv("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST");
  google::cloud::internal::SetEnv("GOOGLE_CLOUD_ENABLE_DIRECT_PATH",
                                  "bigtable-mismatch,bigtable");

  EXPECT_EQ("directpath-bigtable.googleapis.com",
            internal::DefaultDataEndpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, DataEmulatorOverridesDirectPath) {
  google::cloud::internal::SetEnv("BIGTABLE_EMULATOR_HOST", "127.0.0.1:1234");
  google::cloud::internal::UnsetEnv("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST");
  google::cloud::internal::SetEnv("GOOGLE_CLOUD_ENABLE_DIRECT_PATH",
                                  "bigtable");

  EXPECT_EQ("127.0.0.1:1234", internal::DefaultDataEndpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, AdminNoEnv) {
  google::cloud::internal::UnsetEnv("BIGTABLE_EMULATOR_HOST");
  google::cloud::internal::UnsetEnv("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST");
  google::cloud::internal::UnsetEnv("GOOGLE_CLOUD_ENABLE_DIRECT_PATH");

  EXPECT_EQ("bigtableadmin.googleapis.com", internal::DefaultAdminEndpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, AdminDirectPathNoEffect) {
  google::cloud::internal::UnsetEnv("BIGTABLE_EMULATOR_HOST");
  google::cloud::internal::UnsetEnv("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST");
  google::cloud::internal::SetEnv("GOOGLE_CLOUD_ENABLE_DIRECT_PATH",
                                  "bigtable");

  EXPECT_EQ("bigtableadmin.googleapis.com", internal::DefaultAdminEndpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, AdminEmulatorOverrides) {
  google::cloud::internal::SetEnv("BIGTABLE_EMULATOR_HOST", "127.0.0.1:1234");
  google::cloud::internal::UnsetEnv("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST");
  google::cloud::internal::UnsetEnv("GOOGLE_CLOUD_ENABLE_DIRECT_PATH");

  EXPECT_EQ("127.0.0.1:1234", internal::DefaultAdminEndpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, AdminInstanceAdminNoEffect) {
  google::cloud::internal::UnsetEnv("BIGTABLE_EMULATOR_HOST");
  google::cloud::internal::SetEnv("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST",
                                  "127.0.0.1:1234");
  google::cloud::internal::UnsetEnv("GOOGLE_CLOUD_ENABLE_DIRECT_PATH");

  EXPECT_EQ("bigtableadmin.googleapis.com", internal::DefaultAdminEndpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, InstanceAdminNoEnv) {
  google::cloud::internal::UnsetEnv("BIGTABLE_EMULATOR_HOST");
  google::cloud::internal::UnsetEnv("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST");
  google::cloud::internal::UnsetEnv("GOOGLE_CLOUD_ENABLE_DIRECT_PATH");

  EXPECT_EQ("bigtableadmin.googleapis.com",
            internal::DefaultInstanceAdminEndpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, InstanceAdminDirectPathNoEffect) {
  google::cloud::internal::UnsetEnv("BIGTABLE_EMULATOR_HOST");
  google::cloud::internal::UnsetEnv("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST");
  google::cloud::internal::SetEnv("GOOGLE_CLOUD_ENABLE_DIRECT_PATH",
                                  "bigtable");

  EXPECT_EQ("bigtableadmin.googleapis.com",
            internal::DefaultInstanceAdminEndpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, InstanceAdminEmulatorOverrides) {
  google::cloud::internal::SetEnv("BIGTABLE_EMULATOR_HOST", "127.0.0.1:1234");
  google::cloud::internal::UnsetEnv("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST");
  google::cloud::internal::UnsetEnv("GOOGLE_CLOUD_ENABLE_DIRECT_PATH");

  EXPECT_EQ("127.0.0.1:1234", internal::DefaultInstanceAdminEndpoint());
}

TEST_F(ClientOptionsDefaultEndpointTest, InstanceAdminInstanceAdminOverrides) {
  google::cloud::internal::SetEnv("BIGTABLE_EMULATOR_HOST", "unused");
  google::cloud::internal::SetEnv("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST",
                                  "127.0.0.1:1234");
  google::cloud::internal::UnsetEnv("GOOGLE_CLOUD_ENABLE_DIRECT_PATH");

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

TEST(ClientOptionsTest, SetGrpclbFallbackTimeoutMS) {
  // Test milliseconds are set properly to channel_arguments
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  ASSERT_STATUS_OK(client_options_object.SetGrpclbFallbackTimeout(
      std::chrono::milliseconds(5)));
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  ASSERT_EQ(4UL, test_args.num_args);
  // Use the low-level C API because grpc::ChannelArguments lacks high-level
  // accessors.
  // test_args now has 3 default arguments. Added max send/receive message size
  // SetGrpclbFallbackTimeout() inserts new argument to args_ hence comparing
  // 4th element of test_args.
  EXPECT_EQ(GRPC_ARG_GRPCLB_FALLBACK_TIMEOUT_MS,
            grpc::string(test_args.args[3].key));
}

TEST(ClientOptionsTest, SetGrpclbFallbackTimeoutSec) {
  // Test seconds are converted into milliseconds
  bigtable::ClientOptions client_options_object_second =
      bigtable::ClientOptions();
  ASSERT_STATUS_OK(client_options_object_second.SetGrpclbFallbackTimeout(
      std::chrono::seconds(5)));
  grpc::ChannelArguments c_args_second =
      client_options_object_second.channel_arguments();
  grpc_channel_args test_args_second = c_args_second.c_channel_args();
  ASSERT_EQ(4UL, test_args_second.num_args);
  EXPECT_EQ(GRPC_ARG_GRPCLB_FALLBACK_TIMEOUT_MS,
            grpc::string(test_args_second.args[3].key));
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
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  ASSERT_EQ(4UL, test_args.num_args);
  // Use the low-level C API because grpc::ChannelArguments lacks high-level
  // accessors.
  // test_args now has 3 default arguments. Added max send/receive message size
  // SetCompressionAlgorithm() inserts new argument to args_ hence comparing 4th
  // element of test_args.
  EXPECT_EQ(GRPC_COMPRESSION_CHANNEL_DEFAULT_ALGORITHM,
            grpc::string(test_args.args[3].key));
}

TEST(ClientOptionsTest, SetMaxReceiveMessageSize) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetMaxReceiveMessageSize(256 * 1024L * 1024L);
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  ASSERT_EQ(4UL, test_args.num_args);
  // Use the low-level C API because grpc::ChannelArguments lacks high-level
  // accessors.
  // test_args now has 3 default arguments. Added max send/receive message size
  // SetMaxReceiveMessageSize() inserts new argument to args_ hence comparing
  // 4th element of test_args.
  EXPECT_EQ(GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH,
            grpc::string(test_args.args[3].key));

  EXPECT_EQ(test_args.args[3].value.integer, (256 * 1024L * 1024L));
}

TEST(ClientOptionsTest, SetMaxSendMessageSize) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetMaxSendMessageSize(256 * 1024L * 1024L);
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  ASSERT_EQ(4UL, test_args.num_args);
  // Use the low-level C API because grpc::ChannelArguments lacks high-level
  // accessors.
  // test_args now has 3 default arguments. Added max send/receive message size
  // SetMaxSendMessageSize() inserts new argument to args_ hence comparing 4th
  // element of test_args.
  EXPECT_EQ(GRPC_ARG_MAX_SEND_MESSAGE_LENGTH,
            grpc::string(test_args.args[3].key));

  EXPECT_EQ(test_args.args[3].value.integer, 256 * 1024L * 1024L);
}

TEST(ClientOptionsTest, SetLoadBalancingPolicyName) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetLoadBalancingPolicyName("test-policy-name");
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  ASSERT_EQ(4UL, test_args.num_args);
  // Use the low-level C API because grpc::ChannelArguments lacks high-level
  // accessors.
  // test_args now has 3 default arguments. Added max send/receive message size
  // SetLoadBalancingPolicyName() inserts new argument to args_ hence comparing
  // 4th element of test_args.
  EXPECT_EQ(GRPC_ARG_LB_POLICY_NAME, grpc::string(test_args.args[3].key));
}

TEST(ClientOptionsTest, SetServiceConfigJSON) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetServiceConfigJSON("test-config");
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  ASSERT_EQ(4UL, test_args.num_args);
  // Use the low-level C API because grpc::ChannelArguments lacks high-level
  // accessors.
  // test_args now has 3 default arguments. Added max send/receive message size
  // SetServiceConfigJSON() inserts new argument to args_ hence comparing 4th
  // element of test_args.
  EXPECT_EQ(GRPC_ARG_SERVICE_CONFIG, grpc::string(test_args.args[3].key));
}

TEST(ClientOptionsTest, SetUserAgentPrefix) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetUserAgentPrefix("test_prefix");
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  ASSERT_EQ(3UL, test_args.num_args);
  // Use the low-level C API because grpc::ChannelArguments lacks high-level
  // accessors.
  // test_args now has 3 default arguments. Added max send/receive message size
  // SetUserAgentPrefix() appends the new prefix to existing prefix hence
  // comparing 1st element of test_args.
  EXPECT_EQ(GRPC_ARG_PRIMARY_USER_AGENT_STRING,
            grpc::string(test_args.args[0].key));
}

TEST(ClientOptionsTest, SetSslTargetNameOverride) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetSslTargetNameOverride("test-name");
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  ASSERT_EQ(4UL, test_args.num_args);
  // Use the low-level C API because grpc::ChannelArguments lacks high-level
  // accessors.
  // test_args now has 3 default arguments. Added max send/receive message size
  // SetSslTargetNameOverride() inserts new argument to args_ hence comparing
  // 4th element of test_args.
  EXPECT_EQ(GRPC_SSL_TARGET_NAME_OVERRIDE_ARG,
            grpc::string(test_args.args[3].key));
}

TEST(ClientOptionsTest, UserAgentPrefix) {
  std::string const actual = bigtable::ClientOptions::UserAgentPrefix();

  EXPECT_THAT(actual, HasSubstr("gcloud-cpp/"));
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
