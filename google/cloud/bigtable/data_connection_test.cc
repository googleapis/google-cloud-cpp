// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include "google/cloud/bigtable/data_connection.h"
#include "google/cloud/bigtable/internal/bigtable_stub_factory.h"
#include "google/cloud/bigtable/internal/data_connection_impl.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/universe_domain_options.h"
#include "rpc_retry_policy.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ms = std::chrono::milliseconds;
using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::IsEmpty;

Options TestOptions() {
  return Options{}
      // Stay on this machine. Don't make a connection to the actual service.
      .set<EndpointOption>("localhost:1")
      .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
      // Disable channel refreshing, which is not under test.
      .set<MaxConnectionRefreshOption>(ms(0))
      // Create the minimum number of stubs.
      .set<GrpcNumChannelsOption>(1)
      // Disable retries
      .set<DataRetryPolicyOption>(
          std::make_shared<DataLimitedErrorCountRetryPolicy>(0));
}

TEST(MakeDataConnection, DefaultsOptions) {
  auto conn = MakeDataConnection(
      TestOptions().set<AppProfileIdOption>("user-supplied"));
  auto options = conn->options();
  EXPECT_TRUE(options.has<DataBackoffPolicyOption>())
      << "Options are not defaulted in MakeDataConnection()";
  EXPECT_EQ(options.get<AppProfileIdOption>(), "user-supplied")
      << "User supplied Options are overridden in MakeDataConnection()";
}

TEST(MakeDataConnection, TracingEnabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto conn = MakeDataConnection(EnableTracing(TestOptions()));
  google::cloud::internal::OptionsSpan o(conn->options());
  (void)conn->Apply("table-name", SingleRowMutation("row"));

  EXPECT_THAT(span_catcher->GetSpans(),
              Contains(SpanNamed("bigtable::Table::Apply")));
}

TEST(MakeDataConnection, TracingDisabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto conn = MakeDataConnection(DisableTracing(TestOptions()));
  google::cloud::internal::OptionsSpan o(conn->options());
  (void)conn->Apply("table-name", SingleRowMutation("row"));

  EXPECT_THAT(span_catcher->GetSpans(), IsEmpty());
}

TEST(MakeDataConnection, WithInstances) {
  InstanceResource instance_a{Project("my-project"), "instance-a"};
  InstanceResource instance_b{Project("my-project"), "instance-b"};
  auto conn = MakeDataConnection(
      {instance_a, instance_b},
      TestOptions().set<AppProfileIdOption>("user-supplied"));
  auto options = conn->options();
  EXPECT_TRUE(options.has<DataBackoffPolicyOption>())
      << "Options are not defaulted in MakeDataConnection()";
  EXPECT_EQ(options.get<AppProfileIdOption>(), "user-supplied")
      << "User supplied Options are overridden in MakeDataConnection()";
  ASSERT_TRUE(options.has<bigtable_internal::InstanceChannelAffinityOption>());
  EXPECT_THAT(
      options.get<bigtable_internal::InstanceChannelAffinityOption>().size(),
      Eq(2));
}

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
TEST(MakeDataConnection, TestingOtelCollectorEnvVar) {
  testing_util::ScopedEnvironment env("GOOGLE_CLOUD_CPP_TESTING_OTEL_COLLECTOR",
                                      "1");
  auto conn = MakeDataConnection(TestOptions().set<EnableMetricsOption>(true));
  EXPECT_NE(conn, nullptr);
}

TEST(MakeDataConnection, DirectPathMetricsModeOptionDisabled) {
  testing_util::ScopedEnvironment env("GOOGLE_CLOUD_CPP_TESTING_OTEL_COLLECTOR",
                                      "1");
  InstanceResource instance_a{Project("my-project"), "instance-a"};
  auto conn = MakeDataConnection(
      {instance_a}, TestOptions()
                        .set<EnableMetricsOption>(true)
                        .set<experimental::DirectPathMetricsModeOption>(
                            experimental::DirectPathMetricsMode::kDisabled));
  EXPECT_NE(conn, nullptr);
}

TEST(MakeDataConnection, DirectPathMetricsModeOptionEnabled) {
  testing_util::ScopedEnvironment emulator("BIGTABLE_EMULATOR_HOST",
                                           std::nullopt);
  testing_util::ScopedEnvironment ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN",
                                     std::nullopt);
  testing_util::ScopedEnvironment direct_path("GOOGLE_CLOUD_ENABLE_DIRECT_PATH",
                                              std::nullopt);
  testing_util::ScopedEnvironment cbt_direct_path("CBT_ENABLE_DIRECTPATH",
                                                  std::nullopt);
  testing_util::ScopedEnvironment env("GOOGLE_CLOUD_CPP_TESTING_OTEL_COLLECTOR",
                                      "1");
  InstanceResource instance_a{Project("my-project"), "instance-a"};
  Options options =
      TestOptions()
          .set<EnableMetricsOption>(true)
          .set<experimental::DirectPathModeOption>(
              experimental::DirectPathMode::kEnabled)
          .set<experimental::DirectPathMetricsModeOption>(
              experimental::DirectPathMetricsMode::kEnabled)
          .set<experimental::DirectPathProbeTimeoutOption>(ms(100));
  options.unset<EndpointOption>();
  auto conn = MakeDataConnection({instance_a}, std::move(options));
  EXPECT_NE(conn, nullptr);
}

TEST(MakeDataConnection, DirectPathModeOptionDisabledNoGrpcMetrics) {
  testing_util::ScopedEnvironment env("GOOGLE_CLOUD_CPP_TESTING_OTEL_COLLECTOR",
                                      "1");
  InstanceResource instance_a{Project("my-project"), "instance-a"};
  auto conn = MakeDataConnection(
      {instance_a}, TestOptions()
                        .set<EnableMetricsOption>(true)
                        .set<experimental::DirectPathModeOption>(
                            experimental::DirectPathMode::kDisabled)
                        .set<experimental::DirectPathMetricsModeOption>(
                            experimental::DirectPathMetricsMode::kEnabled));
  EXPECT_NE(conn, nullptr);
}

TEST(MakeDataConnection, DirectPathInitializationModeBlocking) {
  testing_util::ScopedEnvironment emulator("BIGTABLE_EMULATOR_HOST",
                                           std::nullopt);
  testing_util::ScopedEnvironment ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN",
                                     std::nullopt);
  testing_util::ScopedEnvironment direct_path("GOOGLE_CLOUD_ENABLE_DIRECT_PATH",
                                              std::nullopt);
  testing_util::ScopedEnvironment cbt_direct_path("CBT_ENABLE_DIRECTPATH",
                                                  std::nullopt);
  InstanceResource instance_a{Project("my-project"), "instance-a"};
  Options options =
      TestOptions()
          .set<experimental::DirectPathModeOption>(
              experimental::DirectPathMode::kEnabled)
          .set<experimental::DirectPathInitializationModeOption>(
              experimental::DirectPathInitializationMode::kBlocking)
          .set<experimental::DirectPathProbeTimeoutOption>(ms(100));
  options.unset<EndpointOption>();
  auto conn = MakeDataConnection({instance_a}, std::move(options));
  EXPECT_NE(conn, nullptr);
}

TEST(MakeDataConnection, DirectPathInitializationModeSpeculative) {
  testing_util::ScopedEnvironment emulator("BIGTABLE_EMULATOR_HOST",
                                           std::nullopt);
  testing_util::ScopedEnvironment ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN",
                                     std::nullopt);
  testing_util::ScopedEnvironment direct_path("GOOGLE_CLOUD_ENABLE_DIRECT_PATH",
                                              std::nullopt);
  testing_util::ScopedEnvironment cbt_direct_path("CBT_ENABLE_DIRECTPATH",
                                                  std::nullopt);
  InstanceResource instance_a{Project("my-project"), "instance-a"};
  Options options =
      TestOptions()
          .set<experimental::DirectPathModeOption>(
              experimental::DirectPathMode::kEnabled)
          .set<experimental::DirectPathInitializationModeOption>(
              experimental::DirectPathInitializationMode::
                  kAsynchronousSpeculative)
          .set<experimental::DirectPathProbeTimeoutOption>(ms(100));
  options.unset<EndpointOption>();
  auto conn = MakeDataConnection({instance_a}, std::move(options));
  EXPECT_NE(conn, nullptr);
}
#endif

TEST(MakeDataConnection, DirectPathDisabledOutsideGoogleDefaultUniverse) {
  testing_util::ScopedEnvironment emulator("BIGTABLE_EMULATOR_HOST",
                                           std::nullopt);
  testing_util::ScopedEnvironment ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN",
                                     std::nullopt);
  testing_util::ScopedEnvironment direct_path("GOOGLE_CLOUD_ENABLE_DIRECT_PATH",
                                              "bigtable");
  testing_util::ScopedEnvironment cbt_direct_path("CBT_ENABLE_DIRECTPATH",
                                                  "true");
  InstanceResource const instance_a{Project("my-project"), "instance-a"};
  Options options =
      TestOptions()
          .set<EnableMetricsOption>(false)
          .set<google::cloud::internal::UniverseDomainOption>("ud.invalid")
          .set<experimental::DirectPathModeOption>(
              experimental::DirectPathMode::kEnabled)
          .set<experimental::DirectPathInitializationModeOption>(
              experimental::DirectPathInitializationMode::kBlocking)
          .set<experimental::DirectPathProbeTimeoutOption>(ms(100));
  options.unset<EndpointOption>();

  std::shared_ptr<DataConnection> blocking_conn =
      MakeDataConnection({instance_a}, options);
  ASSERT_THAT(blocking_conn, testing::NotNull());
  EXPECT_THAT(blocking_conn->options().get<EndpointOption>(),
              Eq("bigtable.ud.invalid"));
  EXPECT_THAT(blocking_conn->options().get<AuthorityOption>(),
              Eq("bigtable.ud.invalid"));

  std::shared_ptr<DataConnection> speculative_conn = MakeDataConnection(
      {instance_a},
      Options(options).set<experimental::DirectPathInitializationModeOption>(
          experimental::DirectPathInitializationMode::
              kAsynchronousSpeculative));
  ASSERT_THAT(speculative_conn, testing::NotNull());
  EXPECT_THAT(speculative_conn->options().get<EndpointOption>(),
              Eq("bigtable.ud.invalid"));
  EXPECT_THAT(speculative_conn->options().get<AuthorityOption>(),
              Eq("bigtable.ud.invalid"));

  std::shared_ptr<DataConnection> single_stub_conn =
      MakeDataConnection(options);
  ASSERT_THAT(single_stub_conn, testing::NotNull());
  EXPECT_THAT(single_stub_conn->options().get<EndpointOption>(),
              Eq("bigtable.ud.invalid"));
  EXPECT_THAT(single_stub_conn->options().get<AuthorityOption>(),
              Eq("bigtable.ud.invalid"));
}

TEST(MakeDataConnection, DirectPathDisabledOutsideGoogleDefaultUniverseEnvVar) {
  testing_util::ScopedEnvironment emulator("BIGTABLE_EMULATOR_HOST",
                                           std::nullopt);
  testing_util::ScopedEnvironment ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN",
                                     "ud-env.invalid");
  testing_util::ScopedEnvironment direct_path("GOOGLE_CLOUD_ENABLE_DIRECT_PATH",
                                              "bigtable");
  testing_util::ScopedEnvironment cbt_direct_path("CBT_ENABLE_DIRECTPATH",
                                                  "true");
  InstanceResource const instance_a{Project("my-project"), "instance-a"};
  Options options =
      TestOptions()
          .set<EnableMetricsOption>(false)
          .set<google::cloud::internal::UniverseDomainOption>("googleapis.com")
          .set<experimental::DirectPathModeOption>(
              experimental::DirectPathMode::kEnabled)
          .set<experimental::DirectPathInitializationModeOption>(
              experimental::DirectPathInitializationMode::kBlocking)
          .set<experimental::DirectPathProbeTimeoutOption>(ms(100));
  options.unset<EndpointOption>();

  std::shared_ptr<DataConnection> blocking_conn =
      MakeDataConnection({instance_a}, options);
  ASSERT_THAT(blocking_conn, testing::NotNull());
  EXPECT_THAT(blocking_conn->options().get<EndpointOption>(),
              Eq("bigtable.ud-env.invalid"));
  EXPECT_THAT(blocking_conn->options().get<AuthorityOption>(),
              Eq("bigtable.ud-env.invalid"));

  std::shared_ptr<DataConnection> speculative_conn = MakeDataConnection(
      {instance_a},
      Options(options).set<experimental::DirectPathInitializationModeOption>(
          experimental::DirectPathInitializationMode::
              kAsynchronousSpeculative));
  ASSERT_THAT(speculative_conn, testing::NotNull());
  EXPECT_THAT(speculative_conn->options().get<EndpointOption>(),
              Eq("bigtable.ud-env.invalid"));
  EXPECT_THAT(speculative_conn->options().get<AuthorityOption>(),
              Eq("bigtable.ud-env.invalid"));

  std::shared_ptr<DataConnection> single_stub_conn =
      MakeDataConnection(options);
  ASSERT_THAT(single_stub_conn, testing::NotNull());
  EXPECT_THAT(single_stub_conn->options().get<EndpointOption>(),
              Eq("bigtable.ud-env.invalid"));
  EXPECT_THAT(single_stub_conn->options().get<AuthorityOption>(),
              Eq("bigtable.ud-env.invalid"));
}

TEST(MakeDataConnection, DirectPathDisabledWithCustomEndpoint) {
  testing_util::ScopedEnvironment emulator("BIGTABLE_EMULATOR_HOST",
                                           std::nullopt);
  testing_util::ScopedEnvironment ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN",
                                     std::nullopt);
  testing_util::ScopedEnvironment direct_path("GOOGLE_CLOUD_ENABLE_DIRECT_PATH",
                                              "bigtable");
  testing_util::ScopedEnvironment cbt_direct_path("CBT_ENABLE_DIRECTPATH",
                                                  "true");
  InstanceResource const instance_a{Project("my-project"), "instance-a"};
  Options const options =
      TestOptions()
          .set<EnableMetricsOption>(false)
          .set<EndpointOption>("custom.tpc.invalid")
          .set<AuthorityOption>("custom.tpc.invalid")
          .set<experimental::DirectPathModeOption>(
              experimental::DirectPathMode::kEnabled)
          .set<experimental::DirectPathInitializationModeOption>(
              experimental::DirectPathInitializationMode::kBlocking)
          .set<experimental::DirectPathProbeTimeoutOption>(ms(100));

  std::shared_ptr<DataConnection> blocking_conn =
      MakeDataConnection({instance_a}, options);
  ASSERT_THAT(blocking_conn, testing::NotNull());
  EXPECT_THAT(blocking_conn->options().get<EndpointOption>(),
              Eq("custom.tpc.invalid"));
  EXPECT_THAT(blocking_conn->options().get<AuthorityOption>(),
              Eq("custom.tpc.invalid"));

  std::shared_ptr<DataConnection> speculative_conn = MakeDataConnection(
      {instance_a},
      Options(options).set<experimental::DirectPathInitializationModeOption>(
          experimental::DirectPathInitializationMode::
              kAsynchronousSpeculative));
  ASSERT_THAT(speculative_conn, testing::NotNull());
  EXPECT_THAT(speculative_conn->options().get<EndpointOption>(),
              Eq("custom.tpc.invalid"));
  EXPECT_THAT(speculative_conn->options().get<AuthorityOption>(),
              Eq("custom.tpc.invalid"));

  std::shared_ptr<DataConnection> single_stub_conn =
      MakeDataConnection(options);
  ASSERT_THAT(single_stub_conn, testing::NotNull());
  EXPECT_THAT(single_stub_conn->options().get<EndpointOption>(),
              Eq("custom.tpc.invalid"));
  EXPECT_THAT(single_stub_conn->options().get<AuthorityOption>(),
              Eq("custom.tpc.invalid"));
}

TEST(MakeDataConnection, DirectPathDisabledWithEmulator) {
  testing_util::ScopedEnvironment emulator("BIGTABLE_EMULATOR_HOST",
                                           "localhost:1");
  testing_util::ScopedEnvironment ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN",
                                     std::nullopt);
  testing_util::ScopedEnvironment direct_path("GOOGLE_CLOUD_ENABLE_DIRECT_PATH",
                                              "bigtable");
  testing_util::ScopedEnvironment cbt_direct_path("CBT_ENABLE_DIRECTPATH",
                                                  "true");
  InstanceResource const instance_a{Project("my-project"), "instance-a"};
  Options options =
      TestOptions()
          .set<EnableMetricsOption>(false)
          .set<experimental::DirectPathModeOption>(
              experimental::DirectPathMode::kEnabled)
          .set<experimental::DirectPathInitializationModeOption>(
              experimental::DirectPathInitializationMode::kBlocking)
          .set<experimental::DirectPathProbeTimeoutOption>(ms(100));
  options.unset<EndpointOption>();

  std::shared_ptr<DataConnection> conn =
      MakeDataConnection({instance_a}, std::move(options));
  ASSERT_THAT(conn, testing::NotNull());
  EXPECT_THAT(conn->options().get<EndpointOption>(), Eq("localhost:1"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
