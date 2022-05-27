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

#include "google/cloud/bigtable/internal/defaults.h"
#include "google/cloud/bigtable/internal/client_options_defaults.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/connection_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::internal::GetIntChannelArgument;
using ::google::cloud::internal::GetStringChannelArgument;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::Contains;
using ::testing::HasSubstr;
using secs = std::chrono::seconds;
using mins = std::chrono::minutes;

TEST(OptionsTest, Defaults) {
  ScopedEnvironment user_project("GOOGLE_CLOUD_CPP_USER_PROJECT",
                                 absl::nullopt);
  ScopedEnvironment emulator_host("BIGTABLE_EMULATOR_HOST", absl::nullopt);
  ScopedEnvironment instance_emulator_host(
      "BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST", absl::nullopt);

  auto opts = DefaultOptions();
  EXPECT_EQ("bigtable.googleapis.com", opts.get<DataEndpointOption>());
  EXPECT_EQ("bigtableadmin.googleapis.com", opts.get<AdminEndpointOption>());
  EXPECT_EQ("bigtableadmin.googleapis.com",
            opts.get<InstanceAdminEndpointOption>());
  EXPECT_EQ(typeid(grpc::GoogleDefaultCredentials()),
            typeid(opts.get<GrpcCredentialOption>()));
  EXPECT_FALSE(opts.has<UserProjectOption>());

  auto args = google::cloud::internal::MakeChannelArguments(opts);
  // Check that the pool domain is not set by default
  auto pool_name = GetIntChannelArgument(args, "cbt-c++/connection-pool-name");
  EXPECT_FALSE(pool_name.has_value());
  // The default must create at least one channel.
  EXPECT_LE(1, opts.get<GrpcNumChannelsOption>());

  auto max_send = GetIntChannelArgument(args, GRPC_ARG_MAX_SEND_MESSAGE_LENGTH);
  ASSERT_TRUE(max_send.has_value());
  EXPECT_EQ(BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH, max_send.value());
  auto max_recv =
      GetIntChannelArgument(args, GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH);
  ASSERT_TRUE(max_recv.has_value());
  EXPECT_EQ(BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH, max_recv.value());

  // See `kDefaultKeepaliveTime`
  // A value lower than 30s might lead to a "too_many_pings" error
  auto time = GetIntChannelArgument(args, GRPC_ARG_KEEPALIVE_TIME_MS);
  ASSERT_TRUE(time.has_value());
  EXPECT_LE(30000, time.value());

  // See `kDefaultKeepaliveTimeout`
  auto timeout = GetIntChannelArgument(args, GRPC_ARG_KEEPALIVE_TIMEOUT_MS);
  ASSERT_TRUE(timeout.has_value());
  EXPECT_EQ(10000, timeout.value());
}

TEST(OptionsTest, DefaultOptionsDoesNotOverride) {
  auto channel_args = grpc::ChannelArguments();
  channel_args.SetString("test-key-1", "value-1");
  auto opts = DefaultOptions(
      Options{}
          .set<DataEndpointOption>("testdata.googleapis.com")
          .set<AdminEndpointOption>("testadmin.googleapis.com")
          .set<InstanceAdminEndpointOption>("testinstanceadmin.googleapis.com")
          .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials())
          .set<GrpcTracingOptionsOption>(
              TracingOptions{}.SetOptions("single_line_mode=F"))
          .set<TracingComponentsOption>({"test-component"})
          .set<GrpcNumChannelsOption>(3)
          .set<GrpcBackgroundThreadPoolSizeOption>(5)
          .set<GrpcChannelArgumentsNativeOption>(channel_args)
          .set<GrpcChannelArgumentsOption>({{"test-key-2", "value-2"}})
          .set<UserAgentProductsOption>({"test-prefix"}));

  EXPECT_EQ("testdata.googleapis.com", opts.get<DataEndpointOption>());
  EXPECT_EQ("testadmin.googleapis.com", opts.get<AdminEndpointOption>());
  EXPECT_EQ("testinstanceadmin.googleapis.com",
            opts.get<InstanceAdminEndpointOption>());
  EXPECT_EQ(typeid(grpc::InsecureChannelCredentials()),
            typeid(opts.get<GrpcCredentialOption>()));
  EXPECT_FALSE(opts.get<GrpcTracingOptionsOption>().single_line_mode());
  EXPECT_THAT(opts.get<TracingComponentsOption>(), Contains("test-component"));
  EXPECT_EQ(3U, opts.get<GrpcNumChannelsOption>());
  EXPECT_EQ(5U, opts.get<GrpcBackgroundThreadPoolSizeOption>());

  auto args = google::cloud::internal::MakeChannelArguments(opts);
  auto key1 = GetStringChannelArgument(args, "test-key-1");
  ASSERT_TRUE(key1.has_value());
  EXPECT_EQ("value-1", key1.value());
  auto key2 = GetStringChannelArgument(args, "test-key-2");
  ASSERT_TRUE(key2.has_value());
  EXPECT_EQ("value-2", key2.value());
  auto s = GetStringChannelArgument(args, GRPC_ARG_PRIMARY_USER_AGENT_STRING);
  ASSERT_TRUE(s.has_value());
  EXPECT_THAT(*s, HasSubstr("test-prefix"));
}

TEST(OptionsTest, EndpointOptionSetsAll) {
  auto options = Options{}.set<EndpointOption>("endpoint-option");
  options = DefaultOptions(std::move(options));
  EXPECT_EQ("endpoint-option", options.get<EndpointOption>());
  EXPECT_EQ("endpoint-option", options.get<DataEndpointOption>());
  EXPECT_EQ("endpoint-option", options.get<AdminEndpointOption>());
  EXPECT_EQ("endpoint-option", options.get<InstanceAdminEndpointOption>());
}

TEST(OptionsTest, EndpointOptionOverridden) {
  auto options = Options{}
                     .set<EndpointOption>("ignored")
                     .set<DataEndpointOption>("data")
                     .set<AdminEndpointOption>("table-admin")
                     .set<InstanceAdminEndpointOption>("instance-admin");
  options = DefaultOptions(std::move(options));
  EXPECT_EQ("data", options.get<DataEndpointOption>());
  EXPECT_EQ("table-admin", options.get<AdminEndpointOption>());
  EXPECT_EQ("instance-admin", options.get<InstanceAdminEndpointOption>());
}

TEST(OptionsTest, DefaultDataOptionsEndpoint) {
  auto options =
      Options{}
          .set<DataEndpointOption>("data.googleapis.com")
          .set<AdminEndpointOption>("tableadmin.googleapis.com")
          .set<InstanceAdminEndpointOption>("instanceadmin.googleapis.com");
  options = DefaultDataOptions(std::move(options));
  EXPECT_EQ("data.googleapis.com", options.get<EndpointOption>());

  options = Options{}.set<EndpointOption>("data.googleapis.com");
  options = DefaultDataOptions(std::move(options));
  EXPECT_EQ("data.googleapis.com", options.get<EndpointOption>());
}

TEST(OptionsTest, DefaultInstanceAdminOptions) {
  auto options =
      Options{}
          .set<DataEndpointOption>("data.googleapis.com")
          .set<AdminEndpointOption>("tableadmin.googleapis.com")
          .set<InstanceAdminEndpointOption>("instanceadmin.googleapis.com");
  options = DefaultInstanceAdminOptions(std::move(options));
  EXPECT_EQ("instanceadmin.googleapis.com", options.get<EndpointOption>());

  options = Options{}.set<EndpointOption>("instanceadmin.googleapis.com");
  options = DefaultInstanceAdminOptions(std::move(options));
  EXPECT_EQ("instanceadmin.googleapis.com", options.get<EndpointOption>());
}

TEST(OptionsTest, DefaultTableAdminOptions) {
  auto options =
      Options{}
          .set<DataEndpointOption>("data.googleapis.com")
          .set<AdminEndpointOption>("tableadmin.googleapis.com")
          .set<InstanceAdminEndpointOption>("instanceadmin.googleapis.com");
  options = DefaultTableAdminOptions(std::move(options));
  EXPECT_EQ("tableadmin.googleapis.com", options.get<EndpointOption>());

  options = Options{}.set<EndpointOption>("tableadmin.googleapis.com");
  options = DefaultTableAdminOptions(std::move(options));
  EXPECT_EQ("tableadmin.googleapis.com", options.get<EndpointOption>());
}

TEST(OptionsTest, DefaultDataOptionsPolicies) {
  auto options = DefaultDataOptions(Options{});
  EXPECT_TRUE(options.has<bigtable_internal::DataRetryPolicyOption>());
  EXPECT_TRUE(options.has<bigtable_internal::DataBackoffPolicyOption>());
  EXPECT_TRUE(options.has<bigtable_internal::IdempotentMutationPolicyOption>());
}

TEST(OptionsTest, DataUserProjectOption) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_CPP_USER_PROJECT", absl::nullopt);
  auto options =
      DefaultDataOptions(Options{}.set<UserProjectOption>("test-project"));
  EXPECT_EQ(options.get<UserProjectOption>(), "test-project");

  env = ScopedEnvironment("GOOGLE_CLOUD_CPP_USER_PROJECT", "env-project");
  options =
      DefaultDataOptions(Options{}.set<UserProjectOption>("test-project"));
  EXPECT_EQ(options.get<UserProjectOption>(), "env-project");
}

TEST(OptionsTest, DataAuthorityOption) {
  auto options = DefaultDataOptions(Options{});
  EXPECT_EQ(options.get<AuthorityOption>(), "bigtable.googleapis.com");

  options = DefaultDataOptions(
      Options{}.set<AuthorityOption>("custom-endpoint.googleapis.com"));
  EXPECT_EQ(options.get<AuthorityOption>(), "custom-endpoint.googleapis.com");
}

TEST(EndpointEnvTest, EmulatorEnvOnly) {
  ScopedEnvironment emulator("BIGTABLE_EMULATOR_HOST", "emulator-host:8000");

  auto opts = DefaultOptions();
  EXPECT_EQ("emulator-host:8000", opts.get<DataEndpointOption>());
  EXPECT_EQ("emulator-host:8000", opts.get<AdminEndpointOption>());
  EXPECT_EQ("emulator-host:8000", opts.get<InstanceAdminEndpointOption>());
}

TEST(EndpointEnvTest, InstanceEmulatorEnvOnly) {
  ScopedEnvironment instance_emulator("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST",
                                      "instance-emulator-host:9000");

  auto opts = DefaultOptions();
  EXPECT_EQ("bigtable.googleapis.com", opts.get<DataEndpointOption>());
  EXPECT_EQ("bigtableadmin.googleapis.com", opts.get<AdminEndpointOption>());
  EXPECT_EQ("instance-emulator-host:9000",
            opts.get<InstanceAdminEndpointOption>());
}

TEST(EndpointEnvTest, InstanceEmulatorEnvOverridesOtherEnv) {
  ScopedEnvironment emulator("BIGTABLE_EMULATOR_HOST", "emulator-host:8000");
  ScopedEnvironment instance_emulator("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST",
                                      "instance-emulator-host:9000");

  auto opts = DefaultOptions();
  EXPECT_EQ("emulator-host:8000", opts.get<DataEndpointOption>());
  EXPECT_EQ("emulator-host:8000", opts.get<AdminEndpointOption>());
  EXPECT_EQ("instance-emulator-host:9000",
            opts.get<InstanceAdminEndpointOption>());
}

TEST(EndpointEnvTest, EmulatorEnvOverridesUserOptions) {
  ScopedEnvironment emulator("BIGTABLE_EMULATOR_HOST", "emulator-host:8000");

  auto opts = DefaultOptions(
      Options{}
          .set<EndpointOption>("ignored-any")
          .set<DataEndpointOption>("ignored-data")
          .set<AdminEndpointOption>("ignored-admin")
          .set<InstanceAdminEndpointOption>("ignored-instance-admin"));

  EXPECT_EQ("emulator-host:8000", opts.get<DataEndpointOption>());
  EXPECT_EQ("emulator-host:8000", opts.get<AdminEndpointOption>());
  EXPECT_EQ("emulator-host:8000", opts.get<InstanceAdminEndpointOption>());
}

TEST(EndpointEnvTest, InstanceEmulatorEnvOverridesUserOption) {
  ScopedEnvironment instance_emulator("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST",
                                      "instance-emulator-host:9000");

  auto opts = DefaultOptions(
      Options{}
          .set<EndpointOption>("ignored-any")
          .set<InstanceAdminEndpointOption>("ignored-instance-admin"));

  EXPECT_EQ("instance-emulator-host:9000",
            opts.get<InstanceAdminEndpointOption>());
}

TEST(EndpointEnvTest, EmulatorEnvDefaultsToInsecureCredentials) {
  ScopedEnvironment emulator("BIGTABLE_EMULATOR_HOST", "emulator-host:8000");

  auto opts = DefaultOptions();
  EXPECT_EQ(typeid(grpc::InsecureChannelCredentials()),
            typeid(opts.get<GrpcCredentialOption>()));
}

TEST(EndpointEnvTest, UserCredentialsOverrideEmulatorEnv) {
  ScopedEnvironment emulator("BIGTABLE_EMULATOR_HOST", "emulator-host:8000");

  auto opts = DefaultOptions(
      Options{}.set<GrpcCredentialOption>(grpc::GoogleDefaultCredentials()));

  EXPECT_EQ(typeid(grpc::GoogleDefaultCredentials()),
            typeid(opts.get<GrpcCredentialOption>()));
}

TEST(ConnectionRefreshRange, BothUnset) {
  auto opts = DefaultOptions();

  // See `kDefaultMinRefreshPeriod`
  EXPECT_LT(absl::FromChrono(secs(15)),
            absl::FromChrono(opts.get<MinConnectionRefreshOption>()));
  // See `kDefaultMaxRefreshPeriod`
  EXPECT_GT(absl::FromChrono(mins(4)),
            absl::FromChrono(opts.get<MaxConnectionRefreshOption>()));
}

TEST(ConnectionRefreshRange, MinSetAboveMaxDefault) {
  auto opts =
      DefaultOptions(Options{}.set<MinConnectionRefreshOption>(mins(10)));

  EXPECT_EQ(absl::FromChrono(mins(10)),
            absl::FromChrono(opts.get<MinConnectionRefreshOption>()));
  EXPECT_EQ(absl::FromChrono(mins(10)),
            absl::FromChrono(opts.get<MaxConnectionRefreshOption>()));
}

TEST(ConnectionRefreshRange, MaxSetBelowMinDefault) {
  auto opts =
      DefaultOptions(Options{}.set<MaxConnectionRefreshOption>(secs(1)));

  EXPECT_EQ(absl::FromChrono(secs(1)),
            absl::FromChrono(opts.get<MinConnectionRefreshOption>()));
  EXPECT_EQ(absl::FromChrono(secs(1)),
            absl::FromChrono(opts.get<MaxConnectionRefreshOption>()));
}

TEST(ConnectionRefreshRange, BothSetValid) {
  auto opts = DefaultOptions(Options{}
                                 .set<MinConnectionRefreshOption>(secs(30))
                                 .set<MaxConnectionRefreshOption>(mins(2)));

  EXPECT_EQ(absl::FromChrono(secs(30)),
            absl::FromChrono(opts.get<MinConnectionRefreshOption>()));
  EXPECT_EQ(absl::FromChrono(mins(2)),
            absl::FromChrono(opts.get<MaxConnectionRefreshOption>()));
}

TEST(ConnectionRefreshRange, BothSetInvalidUsesMax) {
  auto opts = DefaultOptions(Options{}
                                 .set<MinConnectionRefreshOption>(mins(2))
                                 .set<MaxConnectionRefreshOption>(secs(30)));

  EXPECT_EQ(absl::FromChrono(mins(2)),
            absl::FromChrono(opts.get<MinConnectionRefreshOption>()));
  EXPECT_EQ(absl::FromChrono(mins(2)),
            absl::FromChrono(opts.get<MaxConnectionRefreshOption>()));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
