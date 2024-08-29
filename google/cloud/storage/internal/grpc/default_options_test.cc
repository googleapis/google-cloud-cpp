// Copyright 2024 Google LLC
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

#include "google/cloud/storage/internal/grpc/default_options.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/internal/grpc/detect_gcp_options.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/detect_gcp.h"
#include "google/cloud/testing_util/chrono_output.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/universe_domain_options.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <memory>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

class MockGcpDetector : public google::cloud::internal::GcpDetector {
 public:
  MOCK_METHOD(bool, IsGoogleCloudBios, (), (override));
  MOCK_METHOD(bool, IsGoogleCloudServerless, (), (override));
};

using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::Return;

google::cloud::Options TestOptions() {
  return Options{}.set<UnifiedCredentialsOption>(MakeInsecureCredentials());
}

TEST(DefaultOptionsGrpc, DefaultOptionsGrpcChannelCount) {
  using ::google::cloud::GrpcNumChannelsOption;
  struct TestCase {
    std::string endpoint;
    int lower_bound;
    int upper_bound;
  } cases[] = {
      {"storage.googleapis.com", 4, std::numeric_limits<int>::max()},
      {"google-c2p:///storage.googleapis.com", 1, 1},
      {"google-c2p-experimental:///storage.googleapis.com", 1, 1},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing with " + test.endpoint);
    auto opts =
        DefaultOptionsGrpc(TestOptions().set<EndpointOption>(test.endpoint));
    auto const count = opts.get<GrpcNumChannelsOption>();
    EXPECT_LE(test.lower_bound, count);
    EXPECT_GE(test.upper_bound, count);

    auto override = DefaultOptionsGrpc(TestOptions()
                                           .set<EndpointOption>(test.endpoint)
                                           .set<GrpcNumChannelsOption>(42));
    EXPECT_EQ(42, override.get<GrpcNumChannelsOption>());
  }
}

TEST(DefaultOptionsGrpc, DefaultEndpointsCloudPath) {
  auto mock = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock, IsGoogleCloudBios()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock, IsGoogleCloudServerless()).WillRepeatedly(Return(false));

  auto options = DefaultOptionsGrpc(Options{}.set<GcpDetectorOption>(mock));
  EXPECT_EQ(options.get<EndpointOption>(), "storage.googleapis.com");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.googleapis.com");
}

TEST(DefaultOptionsGrpc, DefaultEndpointsDirectPath) {
  auto mock = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock, IsGoogleCloudBios()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock, IsGoogleCloudServerless()).WillRepeatedly(Return(true));

  auto options = DefaultOptionsGrpc(Options{}.set<GcpDetectorOption>(mock));
  EXPECT_EQ(options.get<EndpointOption>(),
            "google-c2p:///storage.googleapis.com");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.googleapis.com");
}

TEST(DefaultOptionsGrpc, EndpointOptionsOverrideDefaults) {
  ScopedEnvironment ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN", "ud-env-var.net");

  auto options = DefaultOptionsGrpc(
      Options{}
          .set<EndpointOption>("from-option")
          .set<AuthorityOption>("host-from-option")
          .set<internal::UniverseDomainOption>("ud-option.net"));
  EXPECT_EQ(options.get<EndpointOption>(), "from-option");
  EXPECT_EQ(options.get<AuthorityOption>(), "host-from-option");
}

TEST(DefaultOptionsGrpc, EnvVarsOverrideOptionsAndDefaults) {
  ScopedEnvironment e("CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT",
                      "from-testbench-env");
  ScopedEnvironment ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN", "ud-env-var.net");

  auto options = DefaultOptionsGrpc(
      Options{}
          .set<EndpointOption>("from-ep-option")
          .set<internal::UniverseDomainOption>("ud-option.net"));
  EXPECT_EQ(options.get<EndpointOption>(), "from-testbench-env");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.ud-env-var.net");
}

TEST(DefaultOptionsGrpc, IncorporatesUniverseDomain) {
  auto options = DefaultOptionsGrpc(
      Options{}.set<internal::UniverseDomainOption>("my-ud.net"));
  EXPECT_EQ(options.get<EndpointOption>(), "storage.my-ud.net");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.my-ud.net");
}

TEST(DefaultOptionsGrpc, IncorporatesUniverseDomainEnvVar) {
  ScopedEnvironment ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN", "ud-env-var.net");

  auto options = DefaultOptionsGrpc(
      Options{}.set<internal::UniverseDomainOption>("ud-option.net"));
  EXPECT_EQ(options.get<EndpointOption>(), "storage.ud-env-var.net");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.ud-env-var.net");
}

TEST(DefaultOptionsGrpc, DefaultOptionsUploadBuffer) {
  auto const with_defaults =
      DefaultOptionsGrpc(Options{}).get<storage::UploadBufferSizeOption>();
  EXPECT_GE(with_defaults, 32 * 1024 * 1024L);

  auto const with_override =
      DefaultOptionsGrpc(
          Options{}.set<storage::UploadBufferSizeOption>(256 * 1024))
          .get<storage::UploadBufferSizeOption>();
  EXPECT_EQ(with_override, 256 * 1024L);
}

TEST(DefaultOptionsGrpc, GrpcEnableMetricsIsSafe) {
  EXPECT_FALSE(GrpcEnableMetricsIsSafe(0, 1, 1));
  EXPECT_FALSE(GrpcEnableMetricsIsSafe(0, 65, 1));
  EXPECT_FALSE(GrpcEnableMetricsIsSafe(1, 62, 0));
  EXPECT_FALSE(GrpcEnableMetricsIsSafe(1, 62, 1));
  EXPECT_FALSE(GrpcEnableMetricsIsSafe(1, 62, 1));
  EXPECT_FALSE(GrpcEnableMetricsIsSafe(1, 63, 0));
  EXPECT_FALSE(GrpcEnableMetricsIsSafe(1, 64, 0));
  EXPECT_TRUE(GrpcEnableMetricsIsSafe(1, 63, 1));
  EXPECT_TRUE(GrpcEnableMetricsIsSafe(1, 63, 2));
  EXPECT_TRUE(GrpcEnableMetricsIsSafe(1, 64, 1));
  EXPECT_TRUE(GrpcEnableMetricsIsSafe(1, 64, 2));
  EXPECT_TRUE(GrpcEnableMetricsIsSafe(1, 65, 0));
  EXPECT_TRUE(GrpcEnableMetricsIsSafe(2, 0, 0));
  EXPECT_TRUE(GrpcEnableMetricsIsSafe(2, 1, 0));
}

TEST(DefaultOptionsGrpc, MetricsEnabled) {
  ScopedEnvironment env("CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT",
                        absl::nullopt);
  auto const options = DefaultOptionsGrpc(Options{});
  auto const expected = GrpcEnableMetricsIsSafe();
  EXPECT_EQ(options.get<storage_experimental::EnableGrpcMetricsOption>(),
            expected);
}

TEST(DefaultOptionsGrpc, MetricsDisabled) {
  ScopedEnvironment env("CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT",
                        "test-only-unused");
  auto const options = DefaultOptionsGrpc(Options{});
  EXPECT_FALSE(options.get<storage_experimental::EnableGrpcMetricsOption>());
}

TEST(DefaultOptionsGrpc, MetricsPeriod) {
  auto const options = DefaultOptionsGrpc(Options{});
  EXPECT_GE(options.get<storage_experimental::GrpcMetricsPeriodOption>(),
            std::chrono::seconds(60));
}

TEST(DefaultOptionsGrpc, MinMetricsPeriod) {
  auto const o0 = DefaultOptionsGrpc(
      Options{}.set<storage_experimental::GrpcMetricsPeriodOption>(
          std::chrono::seconds(0)));
  EXPECT_GT(o0.get<storage_experimental::GrpcMetricsPeriodOption>(),
            std::chrono::seconds(0));

  auto const m5 = DefaultOptionsGrpc(
      Options{}.set<storage_experimental::GrpcMetricsPeriodOption>(
          std::chrono::seconds(-5)));
  EXPECT_GT(m5.get<storage_experimental::GrpcMetricsPeriodOption>(),
            std::chrono::seconds(0));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
