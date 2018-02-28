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

#include "bigtable/client/client_options.h"
#include <gmock/gmock.h>
#include <cstdlib>

#ifdef WIN32
// We need _putenv_s(), which is defined here:
#include <stdlib.h>
#else
// On Unix-like systems we need setenv()/unsetenv(), which are defined here:
#include <unistd.h>
#endif  // WIN32

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

namespace {
void UnsetEnv(char const* variable) {
#ifdef WIN32
  // Use _putenv_s() instead of SetEnvironmentVariable() because std::getenv()
  // caches the environment during program startup.
  (void)_putenv_s(variable, "");
#else
  unsetenv(variable);
#endif  // WIN32
}

void SetEnv(char const* variable, char const* value) {
#ifdef WIN32
  // Use _putenv_s() instead of SetEnvironmentVariable() because std::getenv()
  // caches the environment during program startup.
  if (value != nullptr) {
    (void)_putenv_s(variable, value);
  } else {
    UnsetEnv(variable);
  }
#else
  (void)setenv(variable, value, 1);
#endif  // WIN32
}

class ClientOptionsEmulatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_ = std::getenv("BIGTABLE_EMULATOR_HOST");
    SetEnv("BIGTABLE_EMULATOR_HOST", "testendpoint.googleapis.com");
  }
  void TearDown() override {
    if (previous_) {
      SetEnv("BIGTABLE_EMULATOR_HOST", previous_);
    } else {
      UnsetEnv("BIGTABLE_EMULATOR_HOST");
    }
  }

 protected:
  char const* previous_ = nullptr;
};
}  // anonymous namespace

TEST_F(ClientOptionsEmulatorTest, Default) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  EXPECT_EQ("testendpoint.googleapis.com",
            client_options_object.data_endpoint());
  EXPECT_EQ("testendpoint.googleapis.com",
            client_options_object.admin_endpoint());
  EXPECT_EQ(typeid(grpc::InsecureChannelCredentials()),
            typeid(client_options_object.credentials()));
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

TEST(ClientOptionsTest, SetGrpclbFallbackTimeoutMS) {
  // Test milliseconds are set properly to channel_arguments
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetGrpclbFallbackTimeout(std::chrono::milliseconds(5));
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  ASSERT_EQ(2UL, test_args.num_args);
  // Use the low-level C API because grpc::ChannelArguments lacks high-level
  // accessors.
  // SetGrpclbFallbackTimeout() inserts new argument to args_ hence comparing
  // 2nd element of test_args.
  EXPECT_EQ(GRPC_ARG_GRPCLB_FALLBACK_TIMEOUT_MS,
            grpc::string(test_args.args[1].key));
}

TEST(ClientOptionsTest, SetGrpclbFallbackTimeoutSec) {
  // Test seconds are converted into milliseconds
  bigtable::ClientOptions client_options_object_second =
      bigtable::ClientOptions();
  client_options_object_second.SetGrpclbFallbackTimeout(
      std::chrono::seconds(5));
  grpc::ChannelArguments c_args_second =
      client_options_object_second.channel_arguments();
  grpc_channel_args test_args_second = c_args_second.c_channel_args();
  ASSERT_EQ(2UL, test_args_second.num_args);
  EXPECT_EQ(GRPC_ARG_GRPCLB_FALLBACK_TIMEOUT_MS,
            grpc::string(test_args_second.args[1].key));
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
TEST(ClientOptionsTest, SetGrpclbFallbackTimeoutException) {
  // Test if fallback_timeout exceeds int range then throw out_of_range
  // exception
  bigtable::ClientOptions client_options_object_third =
      bigtable::ClientOptions();
  EXPECT_THROW(client_options_object_third.SetGrpclbFallbackTimeout(
                   std::chrono::hours(999)),
               std::range_error);
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

TEST(ClientOptionsTest, SetCompressionAlgorithm) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetCompressionAlgorithm(GRPC_COMPRESS_NONE);
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  ASSERT_EQ(2UL, test_args.num_args);
  // Use the low-level C API because grpc::ChannelArguments lacks high-level
  // accessors.
  // SetCompressionAlgorithm() inserts new argument to args_ hence comparing 2nd
  // element of test_args.
  EXPECT_EQ(GRPC_COMPRESSION_CHANNEL_DEFAULT_ALGORITHM,
            grpc::string(test_args.args[1].key));
}

TEST(ClientOptionsTest, SetMaxReceiveMessageSize) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetMaxReceiveMessageSize(5);
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  ASSERT_EQ(2UL, test_args.num_args);
  // Use the low-level C API because grpc::ChannelArguments lacks high-level
  // accessors.
  // SetMaxReceiveMessageSize() inserts new argument to args_ hence comparing
  // 2nd element of test_args.
  EXPECT_EQ(GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH,
            grpc::string(test_args.args[1].key));
}

TEST(ClientOptionsTest, SetMaxSendMessageSize) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetMaxSendMessageSize(5);
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  ASSERT_EQ(2UL, test_args.num_args);
  // Use the low-level C API because grpc::ChannelArguments lacks high-level
  // accessors.
  // SetMaxSendMessageSize() inserts new argument to args_ hence comparing 2nd
  // element of test_args.
  EXPECT_EQ(GRPC_ARG_MAX_SEND_MESSAGE_LENGTH,
            grpc::string(test_args.args[1].key));
}

TEST(ClientOptionsTest, SetLoadBalancingPolicyName) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetLoadBalancingPolicyName("test-policy-name");
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  ASSERT_EQ(2UL, test_args.num_args);
  // Use the low-level C API because grpc::ChannelArguments lacks high-level
  // accessors.
  // SetLoadBalancingPolicyName() inserts new argument to args_ hence comparing
  // 2nd element of test_args.
  EXPECT_EQ(GRPC_ARG_LB_POLICY_NAME, grpc::string(test_args.args[1].key));
}

TEST(ClientOptionsTest, SetServiceConfigJSON) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetServiceConfigJSON("test-config");
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  ASSERT_EQ(2UL, test_args.num_args);
  // Use the low-level C API because grpc::ChannelArguments lacks high-level
  // accessors.
  // SetServiceConfigJSON() inserts new argument to args_ hence comparing 2nd
  // element of test_args.
  EXPECT_EQ(GRPC_ARG_SERVICE_CONFIG, grpc::string(test_args.args[1].key));
}

TEST(ClientOptionsTest, SetUserAgentPrefix) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetUserAgentPrefix("test_prefix");
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  ASSERT_EQ(1UL, test_args.num_args);
  // Use the low-level C API because grpc::ChannelArguments lacks high-level
  // accessors.
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
  ASSERT_EQ(2UL, test_args.num_args);
  // Use the low-level C API because grpc::ChannelArguments lacks high-level
  // accessors.
  // SetSslTargetNameOverride() inserts new argument to args_ hence comparing
  // 2nd element of test_args.
  EXPECT_EQ(GRPC_SSL_TARGET_NAME_OVERRIDE_ARG,
            grpc::string(test_args.args[1].key));
}
