// Copyright 2021 Google LLC
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

#include "google/cloud/spanner/internal/defaults.h"
#include "google/cloud/spanner/internal/session_pool.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/spanner/session_pool_options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/compiler_info.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::StartsWith;

// Returns a matcher that will match our user-agent string. This is a lambda so
// we don't have to spell the exact gMock matcher type being returned.
auto gcloud_user_agent_matcher = [] {
  return AllOf(StartsWith("gcloud-cpp/" + VersionString()),
               HasSubstr(google::cloud::internal::CompilerId()),
               HasSubstr(google::cloud::internal::CompilerVersion()),
               HasSubstr(google::cloud::internal::CompilerFeatures()));
};

TEST(Options, Defaults) {
  testing_util::ScopedEnvironment endpoint_env(
      "GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT", absl::nullopt);
  testing_util::ScopedEnvironment tracing_components_env(
      "GOOGLE_CLOUD_CPP_ENABLE_TRACING", absl::nullopt);
  testing_util::ScopedEnvironment tracing_options_env(
      "GOOGLE_CLOUD_CPP_TRACING_OPTIONS", absl::nullopt);
  testing_util::ScopedEnvironment emulator_env("SPANNER_EMULATOR_HOST",
                                               absl::nullopt);
  auto opts = spanner_internal::DefaultOptions();

  EXPECT_EQ(opts.get<EndpointOption>(), "spanner.googleapis.com");
  // In Google's testing environment `expected` can be `nullptr`, we just want
  // to verify that both are `nullptr` or neither is `nullptr`.
  auto const expected = grpc::GoogleDefaultCredentials();
  EXPECT_EQ(!!opts.get<GrpcCredentialOption>(), !!expected);
  EXPECT_EQ(opts.get<GrpcNumChannelsOption>(), 4);
  EXPECT_THAT(opts.get<TracingComponentsOption>(), IsEmpty());
  EXPECT_EQ(opts.get<GrpcTracingOptionsOption>(), TracingOptions{});
  EXPECT_THAT(opts.get<UserAgentProductsOption>(),
              ElementsAre(gcloud_user_agent_matcher()));

  EXPECT_EQ(0, opts.get<SessionPoolMinSessionsOption>());
  EXPECT_EQ(100, opts.get<SessionPoolMaxSessionsPerChannelOption>());
  EXPECT_EQ(0, opts.get<SessionPoolMaxIdleSessionsOption>());
  EXPECT_EQ(ActionOnExhaustion::kBlock,
            opts.get<SessionPoolActionOnExhaustionOption>());
  EXPECT_EQ(std::chrono::minutes(55),
            opts.get<SessionPoolKeepAliveIntervalOption>());

  EXPECT_TRUE(opts.has<SpannerRetryPolicyOption>());
  EXPECT_TRUE(opts.has<SpannerBackoffPolicyOption>());
  EXPECT_TRUE(opts.has<spanner_internal::SessionPoolClockOption>());
}

TEST(Options, AdminDefaults) {
  testing_util::ScopedEnvironment endpoint_env(
      "GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT", absl::nullopt);
  testing_util::ScopedEnvironment tracing_components_env(
      "GOOGLE_CLOUD_CPP_ENABLE_TRACING", absl::nullopt);
  testing_util::ScopedEnvironment tracing_options_env(
      "GOOGLE_CLOUD_CPP_TRACING_OPTIONS", absl::nullopt);
  testing_util::ScopedEnvironment emulator_env("SPANNER_EMULATOR_HOST",
                                               absl::nullopt);
  auto opts = spanner_internal::DefaultAdminOptions();

  EXPECT_EQ(opts.get<EndpointOption>(), "spanner.googleapis.com");
  // In Google's testing environment `expected` can be `nullptr`, we just want
  // to verify that both are `nullptr` or neither is `nullptr`.
  auto const expected = grpc::GoogleDefaultCredentials();
  EXPECT_EQ(!!opts.get<GrpcCredentialOption>(), !!expected);
  EXPECT_EQ(opts.get<GrpcNumChannelsOption>(), 4);
  EXPECT_THAT(opts.get<TracingComponentsOption>(), IsEmpty());
  EXPECT_EQ(opts.get<GrpcTracingOptionsOption>(), TracingOptions{});
  EXPECT_THAT(opts.get<UserAgentProductsOption>(),
              ElementsAre(gcloud_user_agent_matcher()));

  EXPECT_TRUE(opts.has<SpannerRetryPolicyOption>());
  EXPECT_TRUE(opts.has<SpannerBackoffPolicyOption>());
  EXPECT_TRUE(opts.has<SpannerPollingPolicyOption>());

  // Admin connections don't use a session pool, so these should not be set.
  EXPECT_FALSE(opts.has<SessionPoolMinSessionsOption>());
  EXPECT_FALSE(opts.has<SessionPoolMaxSessionsPerChannelOption>());
  EXPECT_FALSE(opts.has<SessionPoolMaxIdleSessionsOption>());
  EXPECT_FALSE(opts.has<SessionPoolActionOnExhaustionOption>());
  EXPECT_FALSE(opts.has<SessionPoolKeepAliveIntervalOption>());
  EXPECT_FALSE(opts.has<spanner_internal::SessionPoolClockOption>());
}

TEST(Options, EndpointFromEnv) {
  testing_util::ScopedEnvironment endpoint_env(
      "GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT", "foo.bar.baz");
  testing_util::ScopedEnvironment emulator_env("SPANNER_EMULATOR_HOST",
                                               absl::nullopt);
  auto opts = spanner_internal::DefaultOptions();
  EXPECT_EQ(opts.get<EndpointOption>(), "foo.bar.baz");
}

TEST(Options, SpannerEmulatorHost) {
  testing_util::ScopedEnvironment endpoint_env(
      "GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT", "no.thank.you");
  testing_util::ScopedEnvironment emulator_env("SPANNER_EMULATOR_HOST",
                                               "foo.bar.baz");
  auto opts = spanner_internal::DefaultOptions();
  EXPECT_EQ(opts.get<EndpointOption>(), "foo.bar.baz");
  EXPECT_NE(opts.get<GrpcCredentialOption>(), nullptr);
}

TEST(Options, TracingComponentsFromEnv) {
  testing_util::ScopedEnvironment tracing_components_env(
      "GOOGLE_CLOUD_CPP_ENABLE_TRACING", "c1,c2,c3");
  auto opts = spanner_internal::DefaultOptions();
  EXPECT_THAT(opts.get<TracingComponentsOption>(),
              ElementsAre("c1", "c2", "c3"));
}

TEST(Options, TracingOptionsFromEnv) {
  testing_util::ScopedEnvironment tracing_components_env(
      "GOOGLE_CLOUD_CPP_TRACING_OPTIONS",
      ",single_line_mode=F"
      ",use_short_repeated_primitives=n"
      ",truncate_string_field_longer_than=256");
  auto opts = spanner_internal::DefaultOptions();
  auto options = opts.get<GrpcTracingOptionsOption>();
  EXPECT_NE(options, TracingOptions{});
  EXPECT_FALSE(options.single_line_mode());
  EXPECT_FALSE(options.use_short_repeated_primitives());
  EXPECT_EQ(256, options.truncate_string_field_longer_than());
}

TEST(Options, PassThroughUnknown) {
  struct UnknownOption {
    using Type = int;
  };
  auto opts = Options{}.set<UnknownOption>(42);
  opts = spanner_internal::DefaultOptions(std::move(opts));
  EXPECT_EQ(42, opts.get<UnknownOption>());
}

TEST(Options, OverrideEndpoint) {
  testing_util::ScopedEnvironment endpoint_env(
      "GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT", "no.thank.you");
  testing_util::ScopedEnvironment emulator_env("SPANNER_EMULATOR_HOST",
                                               absl::nullopt);
  auto opts = Options{}.set<EndpointOption>("foo.bar.baz");
  opts = spanner_internal::DefaultOptions(std::move(opts));
  EXPECT_EQ("foo.bar.baz", opts.get<EndpointOption>());
}

TEST(Options, OverrideCredential) {
  auto cred = grpc::InsecureChannelCredentials();
  auto opts = Options{}.set<GrpcCredentialOption>(cred);
  opts = spanner_internal::DefaultOptions(std::move(opts));
  EXPECT_EQ(cred.get(), opts.get<GrpcCredentialOption>().get());
}

TEST(Options, OverrideBackgroundThreadsFactory) {
  bool called = false;
  auto factory = [&called] {
    called = true;
    return internal::MakeBackgroundThreadsFactory()();
  };
  auto opts =
      Options{}.set<GrpcBackgroundThreadsFactoryOption>(std::move(factory));
  opts = spanner_internal::DefaultOptions(std::move(opts));
  called = false;
  opts.get<GrpcBackgroundThreadsFactoryOption>()();
  EXPECT_TRUE(called);
}

TEST(Options, OverrideNumChannels) {
  auto opts = Options{}.set<GrpcNumChannelsOption>(42);
  opts = spanner_internal::DefaultOptions(std::move(opts));
  EXPECT_EQ(42, opts.get<GrpcNumChannelsOption>());
}

TEST(Options, OverrideTracingComponents) {
  testing_util::ScopedEnvironment tracing_components_env(
      "GOOGLE_CLOUD_CPP_ENABLE_TRACING", "n1,n2,n3");
  std::set<std::string> components{"c1", "c2", "c3"};
  auto opts = Options{}.set<TracingComponentsOption>(components);
  opts = spanner_internal::DefaultOptions(std::move(opts));
  EXPECT_THAT(opts.get<TracingComponentsOption>(),
              ElementsAre("c1", "c2", "c3"));
}

TEST(Options, OverrideTracingOptions) {
  testing_util::ScopedEnvironment tracing_components_env(
      "GOOGLE_CLOUD_CPP_TRACING_OPTIONS",
      ",single_line_mode=T"
      ",use_short_repeated_primitives=y"
      ",truncate_string_field_longer_than=128");
  auto options = TracingOptions().SetOptions(
      ",single_line_mode=F"
      ",use_short_repeated_primitives=n"
      ",truncate_string_field_longer_than=256");
  auto opts = Options{}.set<GrpcTracingOptionsOption>(std::move(options));
  opts = spanner_internal::DefaultOptions(std::move(opts));
  options = opts.get<GrpcTracingOptionsOption>();
  EXPECT_NE(options, TracingOptions{});
  EXPECT_FALSE(options.single_line_mode());
  EXPECT_FALSE(options.use_short_repeated_primitives());
  EXPECT_EQ(256, options.truncate_string_field_longer_than());
}

TEST(Options, AppendToUserAgent) {
  Options opts;
  opts.lookup<UserAgentProductsOption>().push_back("product-a/1.2.3");
  opts.lookup<UserAgentProductsOption>().push_back("product-b/4.5.6");

  opts = spanner_internal::DefaultOptions(std::move(opts));
  // The gcloud user-agent string should be first.
  EXPECT_THAT(opts.get<UserAgentProductsOption>(),
              ElementsAre(gcloud_user_agent_matcher(), "product-a/1.2.3",
                          "product-b/4.5.6"));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
