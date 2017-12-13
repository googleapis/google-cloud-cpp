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

TEST(ClientOptionsTest, ClientOptionsDefaultSettings) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  EXPECT_EQ("bigtable.googleapis.com", client_options_object.endpoint());
  EXPECT_EQ(typeid(grpc::GoogleDefaultCredentials()),
            typeid(client_options_object.credentials()));
}

TEST(ClientOptionsTest, ClientOptionsCustomEndpoint) {
  setenv("BIGTABLE_EMULATOR_HOST", "testendpoint.googleapis.com", 1);
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  EXPECT_EQ("testendpoint.googleapis.com", client_options_object.endpoint());
  EXPECT_EQ(typeid(grpc::InsecureChannelCredentials()),
            typeid(client_options_object.credentials()));
  unsetenv("BIGTABLE_EMULATOR_HOST");
}

TEST(ClientOptionsTest, EditEndpoint) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object =
      client_options_object.SetEndpoint("customendpoint.com");
  EXPECT_EQ("customendpoint.com", client_options_object.endpoint());
}

TEST(ClientOptionsTest, EditCredentials) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object =
      client_options_object.SetCredentials(grpc::InsecureChannelCredentials());
  EXPECT_EQ(typeid(grpc::InsecureChannelCredentials()),
            typeid(client_options_object.credentials()));
}

TEST(ClientOptionsTest, SetGrpclbFallbackTimeout) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetGrpclbFallbackTimeout(5);
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  EXPECT_EQ(2, test_args.num_args);
  EXPECT_EQ(5, test_args.args[1].value.integer);
}

TEST(ClientOptionsTest, SetCompressionAlgorithm) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetCompressionAlgorithm(GRPC_COMPRESS_NONE);
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  EXPECT_EQ(2, test_args.num_args);
  EXPECT_EQ(GRPC_COMPRESS_NONE, test_args.args[1].value.integer);
}

TEST(ClientOptionsTest, SetMaxReceiveMessageSize) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetMaxReceiveMessageSize(5);
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  EXPECT_EQ(2, test_args.num_args);
  EXPECT_EQ(5, test_args.args[1].value.integer);
}

TEST(ClientOptionsTest, SetMaxSendMessageSize) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetMaxSendMessageSize(5);
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  EXPECT_EQ(2, test_args.num_args);
  EXPECT_EQ(5, test_args.args[1].value.integer);
}

TEST(ClientOptionsTest, SetLoadBalancingPolicyName) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetLoadBalancingPolicyName("test-policy-name");
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  EXPECT_EQ(2, test_args.num_args);
}

TEST(ClientOptionsTest, SetServiceConfigJSON) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetServiceConfigJSON("test-config");
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  EXPECT_EQ(2, test_args.num_args);
}

TEST(ClientOptionsTest, SetUserAgentPrefix) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetUserAgentPrefix("test_prefix");
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  EXPECT_EQ(1, test_args.num_args);
}

TEST(ClientOptionsTest, SetSslTargetNameOverride) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object.SetSslTargetNameOverride("test-name");
  grpc::ChannelArguments c_args = client_options_object.channel_arguments();
  grpc_channel_args test_args = c_args.c_channel_args();
  EXPECT_EQ(2, test_args.num_args);
}
