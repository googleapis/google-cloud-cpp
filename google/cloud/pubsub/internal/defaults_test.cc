// Copyright 2021 Google LLC
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

#include "google/cloud/pubsub/internal/defaults.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/connection_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/user_agent_prefix.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>
#include <chrono>
#include <limits>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ms = std::chrono::milliseconds;
using ::google::cloud::testing_util::ScopedEnvironment;
using std::chrono::seconds;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;

TEST(OptionsTest, SetEmulatorEnvOverrides) {
  ScopedEnvironment emulator("PUBSUB_EMULATOR_HOST", "override-test-endpoint");
  auto opts = DefaultCommonOptions(
      Options{}
          .set<EndpointOption>("ignored-endpoint")
          .set<GrpcCredentialOption>(grpc::GoogleDefaultCredentials()));
  EXPECT_EQ("override-test-endpoint", opts.get<EndpointOption>());
  EXPECT_EQ(typeid(grpc::InsecureChannelCredentials()),
            typeid(opts.get<GrpcCredentialOption>()));
}

TEST(OptionsTest, UnsetEmulatorEnv) {
  ScopedEnvironment emulator("PUBSUB_EMULATOR_HOST", absl::nullopt);
  auto opts = DefaultCommonOptions(
      Options{}
          .set<EndpointOption>("used-endpoint")
          .set<GrpcCredentialOption>(grpc::GoogleDefaultCredentials()));
  EXPECT_EQ("used-endpoint", opts.get<EndpointOption>());
  EXPECT_EQ(typeid(grpc::GoogleDefaultCredentials()),
            typeid(opts.get<GrpcCredentialOption>()));
}

TEST(OptionsTest, CommonDefaults) {
  auto opts = DefaultCommonOptions(Options{});
  EXPECT_EQ("pubsub.googleapis.com", opts.get<EndpointOption>());
  EXPECT_EQ(typeid(grpc::GoogleDefaultCredentials()),
            typeid(opts.get<GrpcCredentialOption>()));
  EXPECT_EQ(static_cast<int>(DefaultThreadCount()),
            opts.get<GrpcNumChannelsOption>());
  EXPECT_EQ(internal::DefaultTracingComponents(),
            opts.get<TracingComponentsOption>());
  EXPECT_EQ(internal::DefaultTracingOptions(),
            opts.get<GrpcTracingOptionsOption>());
  EXPECT_TRUE(opts.has<pubsub::RetryPolicyOption>());
  EXPECT_TRUE(opts.has<pubsub::BackoffPolicyOption>());
  EXPECT_EQ(DefaultThreadCount(),
            opts.get<GrpcBackgroundThreadPoolSizeOption>());
  EXPECT_THAT(opts.get<UserAgentProductsOption>(),
              ElementsAre(internal::UserAgentPrefix()));
}

TEST(OptionsTest, CommonConstraints) {
  auto opts = DefaultCommonOptions(Options{}.set<GrpcNumChannelsOption>(-1));
  EXPECT_LT(0, opts.get<GrpcNumChannelsOption>());

  opts = DefaultCommonOptions(Options{}.set<GrpcNumChannelsOption>(0));
  EXPECT_LT(0, opts.get<GrpcNumChannelsOption>());
}

TEST(OptionsTest, UserSetCommonOptions) {
  auto channel_args = grpc::ChannelArguments();
  channel_args.SetString("test-key-1", "value-1");
  auto opts = DefaultCommonOptions(
      Options{}
          .set<EndpointOption>("test-endpoint")
          .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials())
          .set<GrpcTracingOptionsOption>(
              TracingOptions{}.SetOptions("single_line_mode=F"))
          .set<TracingComponentsOption>({"test-component"})
          .set<GrpcNumChannelsOption>(3)
          .set<GrpcBackgroundThreadPoolSizeOption>(5)
          .set<GrpcChannelArgumentsNativeOption>(channel_args)
          .set<GrpcChannelArgumentsOption>({{"test-key-2", "value-2"}})
          .set<UserAgentProductsOption>({"test-prefix"}));

  EXPECT_EQ(typeid(grpc::InsecureChannelCredentials()),
            typeid(opts.get<GrpcCredentialOption>()));
  EXPECT_FALSE(opts.get<GrpcTracingOptionsOption>().single_line_mode());
  EXPECT_THAT(opts.get<TracingComponentsOption>(), Contains("test-component"));
  EXPECT_EQ(3U, opts.get<GrpcNumChannelsOption>());
  EXPECT_EQ(5U, opts.get<GrpcBackgroundThreadPoolSizeOption>());

  auto args = internal::MakeChannelArguments(opts);
  auto key1 = internal::GetStringChannelArgument(args, "test-key-1");
  ASSERT_TRUE(key1.has_value());
  EXPECT_EQ("value-1", key1.value());
  auto key2 = internal::GetStringChannelArgument(args, "test-key-2");
  ASSERT_TRUE(key2.has_value());
  EXPECT_EQ("value-2", key2.value());
  auto s = internal::GetStringChannelArgument(
      args, GRPC_ARG_PRIMARY_USER_AGENT_STRING);
  ASSERT_TRUE(s.has_value());
  EXPECT_THAT(*s, HasSubstr("test-prefix"));
}

TEST(OptionsTest, PublisherDefaults) {
  auto opts = DefaultPublisherOptions(Options{});
  EXPECT_EQ(ms(10), opts.get<pubsub::MaxHoldTimeOption>());
  EXPECT_EQ(100, opts.get<pubsub::MaxBatchMessagesOption>());
  EXPECT_EQ(1024 * 1024L, opts.get<pubsub::MaxBatchBytesOption>());
  EXPECT_EQ((std::numeric_limits<std::size_t>::max)(),
            opts.get<pubsub::MaxPendingBytesOption>());
  EXPECT_EQ((std::numeric_limits<std::size_t>::max)(),
            opts.get<pubsub::MaxPendingMessagesOption>());
  EXPECT_FALSE(opts.get<pubsub::MessageOrderingOption>());
  EXPECT_EQ(pubsub::FullPublisherAction::kBlocks,
            opts.get<pubsub::FullPublisherActionOption>());
  EXPECT_EQ(GRPC_COMPRESS_DEFLATE,
            opts.get<pubsub::CompressionAlgorithmOption>());
  EXPECT_FALSE(opts.has<pubsub::CompressionThresholdOption>());
}

TEST(OptionsTest, UserSetPublisherOptions) {
  auto opts =
      DefaultPublisherOptions(Options{}
                                  .set<pubsub::MaxHoldTimeOption>(ms(100))
                                  .set<pubsub::MaxBatchMessagesOption>(1)
                                  .set<pubsub::MaxBatchBytesOption>(2)
                                  .set<pubsub::MaxPendingBytesOption>(3)
                                  .set<pubsub::MaxPendingMessagesOption>(4)
                                  .set<pubsub::MessageOrderingOption>(true)
                                  .set<pubsub::FullPublisherActionOption>(
                                      pubsub::FullPublisherAction::kIgnored));

  EXPECT_EQ(ms(100), opts.get<pubsub::MaxHoldTimeOption>());
  EXPECT_EQ(1U, opts.get<pubsub::MaxBatchMessagesOption>());
  EXPECT_EQ(2U, opts.get<pubsub::MaxBatchBytesOption>());
  EXPECT_EQ(3U, opts.get<pubsub::MaxPendingBytesOption>());
  EXPECT_EQ(4U, opts.get<pubsub::MaxPendingMessagesOption>());
  EXPECT_TRUE(opts.get<pubsub::MessageOrderingOption>());
  EXPECT_EQ(pubsub::FullPublisherAction::kIgnored,
            opts.get<pubsub::FullPublisherActionOption>());
}

TEST(OptionsTest, SubscriberDefaults) {
  auto opts = DefaultSubscriberOptions(Options{});
  EXPECT_EQ(seconds(0), opts.get<pubsub::MaxDeadlineTimeOption>());
  EXPECT_EQ(seconds(600), opts.get<pubsub::MaxDeadlineExtensionOption>());
  EXPECT_EQ(1000, opts.get<pubsub::MaxOutstandingMessagesOption>());
  EXPECT_EQ(100 * 1024 * 1024L, opts.get<pubsub::MaxOutstandingBytesOption>());
  EXPECT_EQ(DefaultThreadCount(), opts.get<pubsub::MaxConcurrencyOption>());

  auto* retry = dynamic_cast<pubsub::LimitedErrorCountRetryPolicy*>(
      opts.get<pubsub::RetryPolicyOption>().get());
  ASSERT_NE(nullptr, retry);
  EXPECT_EQ((std::numeric_limits<int>::max)(), retry->maximum_failures());
}

TEST(OptionsTest, SubscriberConstraints) {
  auto opts = DefaultSubscriberOptions(
      Options{}
          .set<pubsub::MaxOutstandingMessagesOption>(-1)
          .set<pubsub::MaxOutstandingBytesOption>(-2)
          .set<pubsub::MaxConcurrencyOption>(0));

  EXPECT_EQ(0, opts.get<pubsub::MaxOutstandingMessagesOption>());
  EXPECT_EQ(0, opts.get<pubsub::MaxOutstandingBytesOption>());
  EXPECT_EQ(DefaultThreadCount(), opts.get<pubsub::MaxConcurrencyOption>());

  opts = DefaultSubscriberOptions(
      Options{}.set<pubsub::MaxDeadlineExtensionOption>(seconds(5)));
  EXPECT_EQ(seconds(10), opts.get<pubsub::MaxDeadlineExtensionOption>());

  opts = DefaultSubscriberOptions(
      Options{}.set<pubsub::MaxDeadlineExtensionOption>(seconds(5000)));
  EXPECT_EQ(seconds(600), opts.get<pubsub::MaxDeadlineExtensionOption>());
}

TEST(OptionsTest, UserSetSubscriberOptions) {
  auto opts = DefaultSubscriberOptions(
      Options{}
          .set<pubsub::MaxDeadlineTimeOption>(seconds(2))
          .set<pubsub::MaxDeadlineExtensionOption>(seconds(30))
          .set<pubsub::MaxOutstandingMessagesOption>(4)
          .set<pubsub::MaxOutstandingBytesOption>(5)
          .set<pubsub::MaxConcurrencyOption>(6));

  EXPECT_EQ(seconds(2), opts.get<pubsub::MaxDeadlineTimeOption>());
  EXPECT_EQ(seconds(30), opts.get<pubsub::MaxDeadlineExtensionOption>());
  EXPECT_EQ(4, opts.get<pubsub::MaxOutstandingMessagesOption>());
  EXPECT_EQ(5, opts.get<pubsub::MaxOutstandingBytesOption>());
  EXPECT_EQ(6, opts.get<pubsub::MaxConcurrencyOption>());
}

TEST(OptionsTest, DefaultSubscriberOnly) {
  // Ensure that we do not set common options
  auto opts = DefaultSubscriberOptionsOnly(Options{});
  EXPECT_FALSE(opts.has<GrpcCredentialOption>());
  EXPECT_FALSE(opts.has<EndpointOption>());
  EXPECT_FALSE(opts.has<GrpcCredentialOption>());
  EXPECT_FALSE(opts.has<GrpcNumChannelsOption>());
  EXPECT_FALSE(opts.has<TracingComponentsOption>());
  EXPECT_FALSE(opts.has<GrpcTracingOptionsOption>());
  EXPECT_FALSE(opts.has<pubsub::BackoffPolicyOption>());
  EXPECT_FALSE(opts.has<GrpcBackgroundThreadPoolSizeOption>());
  EXPECT_FALSE(opts.has<UserAgentProductsOption>());

  // Ensure that we do set common options
  opts = DefaultSubscriberOptions(Options{});
  EXPECT_TRUE(opts.has<GrpcCredentialOption>());
  EXPECT_TRUE(opts.has<EndpointOption>());
  EXPECT_TRUE(opts.has<GrpcCredentialOption>());
  EXPECT_TRUE(opts.has<GrpcNumChannelsOption>());
  EXPECT_TRUE(opts.has<TracingComponentsOption>());
  EXPECT_TRUE(opts.has<GrpcTracingOptionsOption>());
  EXPECT_TRUE(opts.has<GrpcBackgroundThreadPoolSizeOption>());
  EXPECT_TRUE(opts.has<UserAgentProductsOption>());
}

TEST(OptionsTest, DefaultPublisherOnly) {
  // Ensure that we do not set common options
  auto opts = DefaultPublisherOptionsOnly(Options{});
  EXPECT_FALSE(opts.has<GrpcCredentialOption>());
  EXPECT_FALSE(opts.has<EndpointOption>());
  EXPECT_FALSE(opts.has<GrpcCredentialOption>());
  EXPECT_FALSE(opts.has<GrpcNumChannelsOption>());
  EXPECT_FALSE(opts.has<TracingComponentsOption>());
  EXPECT_FALSE(opts.has<GrpcTracingOptionsOption>());
  EXPECT_FALSE(opts.has<GrpcBackgroundThreadPoolSizeOption>());
  EXPECT_FALSE(opts.has<UserAgentProductsOption>());

  // Ensure that we do set common options
  opts = DefaultPublisherOptions(Options{});
  EXPECT_TRUE(opts.has<GrpcCredentialOption>());
  EXPECT_TRUE(opts.has<EndpointOption>());
  EXPECT_TRUE(opts.has<GrpcCredentialOption>());
  EXPECT_TRUE(opts.has<GrpcNumChannelsOption>());
  EXPECT_TRUE(opts.has<TracingComponentsOption>());
  EXPECT_TRUE(opts.has<GrpcTracingOptionsOption>());
  EXPECT_TRUE(opts.has<GrpcBackgroundThreadPoolSizeOption>());
  EXPECT_TRUE(opts.has<UserAgentProductsOption>());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
