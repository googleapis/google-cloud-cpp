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
#include "google/cloud/storage/options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/chrono_output.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/universe_domain_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::ScopedEnvironment;

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

TEST(DefaultOptionsGrpc, DefaultEndpoints) {
  auto options = DefaultOptionsGrpc();
  EXPECT_EQ(options.get<EndpointOption>(), "storage.googleapis.com");
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

TEST(DefaultOptionsGrpc, MetricsEnabled) {
  auto const options = DefaultOptionsGrpc(Options{});
  EXPECT_TRUE(options.get<storage_experimental::EnableGrpcMetrics>());
}

TEST(DefaultOptionsGrpc, MetricsPeriod) {
  auto const options = DefaultOptionsGrpc(Options{});
  EXPECT_GE(options.get<storage_experimental::GrpcMetricsPeriodOption>(),
            std::chrono::seconds(60));
}

TEST(DefaultOptionsGrpc, MinMetricsPeriod) {
  auto const o0 =
      DefaultOptionsGrpc(Options{}.set<storage_experimental::GrpcMetricsPeriodOption>(
          std::chrono::seconds(0)));
  EXPECT_GT(o0.get<storage_experimental::GrpcMetricsPeriodOption>(),
            std::chrono::seconds(0));

  auto const m5 =
      DefaultOptionsGrpc(Options{}.set<storage_experimental::GrpcMetricsPeriodOption>(
          std::chrono::seconds(-5)));
  EXPECT_GT(m5.get<storage_experimental::GrpcMetricsPeriodOption>(),
            std::chrono::seconds(0));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
