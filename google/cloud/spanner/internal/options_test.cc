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

#include "google/cloud/spanner/internal/options.h"
#include "google/cloud/spanner/session_pool_options.h"
#include "google/cloud/internal/common_options.h"
#include "google/cloud/internal/compiler_info.h"
#include "google/cloud/internal/grpc_options.h"
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
  auto opts = spanner_internal::DefaultOptions();
  EXPECT_EQ(opts.get<internal::EndpointOption>(), "spanner.googleapis.com");
  EXPECT_NE(opts.get<internal::GrpcCredentialOption>(), nullptr);
  EXPECT_NE(opts.get<internal::GrpcBackgroundThreadsFactoryOption>(), nullptr);
  EXPECT_EQ(opts.get<internal::GrpcNumChannelsOption>(), 4);
  EXPECT_THAT(opts.get<internal::UserAgentProductsOption>(),
              ElementsAre(gcloud_user_agent_matcher()));

  EXPECT_EQ(0, opts.get<spanner_internal::SessionPoolMinSessionsOption>());
  EXPECT_EQ(
      100,
      opts.get<spanner_internal::SessionPoolMaxSessionsPerChannelOption>());
  EXPECT_EQ(0, opts.get<spanner_internal::SessionPoolMaxIdleSessionsOption>());
  EXPECT_EQ(ActionOnExhaustion::kBlock,
            opts.get<spanner_internal::SessionPoolActionOnExhaustionOption>());
  EXPECT_EQ(std::chrono::minutes(55),
            opts.get<spanner_internal::SessionPoolKeepAliveIntervalOption>());
}

TEST(Options, EndpointFromEnv) {
  testing_util::ScopedEnvironment env(
      "GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT", "foo.bar.baz");
  auto opts = spanner_internal::DefaultOptions();
  EXPECT_EQ(opts.get<internal::EndpointOption>(), "foo.bar.baz");
}

TEST(Options, SpannerEmulatorHost) {
  testing_util::ScopedEnvironment env("SPANNER_EMULATOR_HOST", "foo.bar.baz");
  auto opts = spanner_internal::DefaultOptions();
  EXPECT_EQ(opts.get<internal::EndpointOption>(), "foo.bar.baz");
  EXPECT_NE(opts.get<internal::GrpcCredentialOption>(), nullptr);
}

TEST(Options, PassThroughUnknown) {
  struct UnknownOption {
    using Type = int;
  };
  auto opts = internal::Options{}.set<UnknownOption>(42);
  opts = spanner_internal::DefaultOptions(std::move(opts));
  EXPECT_EQ(42, opts.get<UnknownOption>());
}

TEST(Options, OverrideEndpoint) {
  auto opts = internal::Options{}.set<internal::EndpointOption>("foo.bar.baz");
  opts = spanner_internal::DefaultOptions(std::move(opts));
  EXPECT_EQ("foo.bar.baz", opts.get<internal::EndpointOption>());
}

TEST(Options, OverrideCredential) {
  auto cred = grpc::InsecureChannelCredentials();
  auto opts = internal::Options{}.set<internal::GrpcCredentialOption>(cred);
  opts = spanner_internal::DefaultOptions(std::move(opts));
  EXPECT_EQ(cred.get(), opts.get<internal::GrpcCredentialOption>().get());
}

TEST(Options, OverrideBackgroundThreadsFactory) {
  bool called = false;
  auto factory = [&called] {
    called = true;
    return internal::DefaultBackgroundThreadsFactory();
  };
  auto opts =
      internal::Options{}.set<internal::GrpcBackgroundThreadsFactoryOption>(
          std::move(factory));
  opts = spanner_internal::DefaultOptions(std::move(opts));
  called = false;
  opts.get<internal::GrpcBackgroundThreadsFactoryOption>()();
  EXPECT_TRUE(called);
}

TEST(Options, OverrideNumChannels) {
  auto opts = internal::Options{}.set<internal::GrpcNumChannelsOption>(42);
  opts = spanner_internal::DefaultOptions(std::move(opts));
  EXPECT_EQ(42, opts.get<internal::GrpcNumChannelsOption>());
}

TEST(Options, AppendToUserAgent) {
  internal::Options opts;
  opts.lookup<internal::UserAgentProductsOption>().push_back("product-a/1.2.3");
  opts.lookup<internal::UserAgentProductsOption>().push_back("product-b/4.5.6");

  opts = spanner_internal::DefaultOptions(std::move(opts));
  // The gcloud user-agent string should be first.
  EXPECT_THAT(opts.get<internal::UserAgentProductsOption>(),
              ElementsAre(gcloud_user_agent_matcher(), "product-a/1.2.3",
                          "product-b/4.5.6"));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
