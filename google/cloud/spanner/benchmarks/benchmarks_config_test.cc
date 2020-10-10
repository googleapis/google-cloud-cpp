// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/benchmarks/benchmarks_config.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner_benchmarks {
inline namespace SPANNER_CLIENT_NS {
namespace {
using ::google::cloud::testing_util::StatusIs;

TEST(BenchmarkConfigTest, ParseAll) {
  auto config = ParseArgs(
      {"placeholder", "--experiment=test-experiment", "--project=test-project",
       "--instance=test-instance", "--samples=50", "--iteration-duration=10",
       "--minimum-threads=1", "--maximum-threads=1", "--minimum-clients=2",
       "--maximum-clients=8", "--table-size=1000", "--query-size=10"});
  ASSERT_STATUS_OK(config);

  EXPECT_EQ("test-experiment", config->experiment);
  EXPECT_EQ("test-project", config->project_id);
  EXPECT_EQ("test-instance", config->instance_id);
  EXPECT_EQ(50, config->samples);
  EXPECT_EQ(10, config->iteration_duration.count());
  EXPECT_EQ(1, config->minimum_threads);
  EXPECT_EQ(1, config->maximum_threads);
  EXPECT_EQ(2, config->minimum_clients);
  EXPECT_EQ(8, config->maximum_clients);
  EXPECT_EQ(1000, config->table_size);
  EXPECT_EQ(10, config->query_size);
}

TEST(BenchmarkConfigTest, ParseNone) {
  testing_util::ScopedEnvironment env("GOOGLE_CLOUD_PROJECT", "test-project");
  auto config = ParseArgs({"placeholder"});
  EXPECT_STATUS_OK(config);
}

TEST(BenchmarkConfigTest, ParseThreads) {
  auto config = ParseArgs({"placeholder", "--project=test-project",
                           "--minimum-threads=4", "--maximum-threads=16"});
  ASSERT_STATUS_OK(config);

  EXPECT_EQ(4, config->minimum_threads);
  EXPECT_EQ(16, config->maximum_threads);
}

TEST(BenchmarkConfigTest, InvalidFlag) {
  auto config = ParseArgs({"placeholder", "--not-a-flag=1"});
  EXPECT_THAT(config, StatusIs(StatusCode::kInvalidArgument));
}

TEST(BenchmarkConfigTest, EmptyExperiment) {
  auto config = ParseArgs({"placeholder", "--experiment="});
  EXPECT_THAT(config, StatusIs(StatusCode::kInvalidArgument));
}

TEST(BenchmarkConfigTest, EmptyProject) {
  auto config = ParseArgs({"placeholder", "--project="});
  EXPECT_THAT(config, StatusIs(StatusCode::kInvalidArgument));
}

TEST(BenchmarkConfigTest, InvalidMinimumThreads) {
  auto config = ParseArgs(
      {"placeholder", "--project=test-project", "--minimum-threads=-7"});
  EXPECT_THAT(config, StatusIs(StatusCode::kInvalidArgument));
}

TEST(BenchmarkConfigTest, InvalidMaximumThreads) {
  auto config = ParseArgs({"placeholder", "--project=test-project",
                           "--minimum-threads=100", "--maximum-threads=5"});
  EXPECT_THAT(config, StatusIs(StatusCode::kInvalidArgument));
}

TEST(BenchmarkConfigTest, InvalidMinimumClients) {
  auto config = ParseArgs(
      {"placeholder", "--project=test-project", "--minimum-clients=-7"});
  EXPECT_THAT(config, StatusIs(StatusCode::kInvalidArgument));
}

TEST(BenchmarkConfigTest, InvalidMaximumClients) {
  auto config = ParseArgs({"placeholder", "--project=test-project",
                           "--minimum-clients=100", "--maximum-clients=5"});
  EXPECT_THAT(config, StatusIs(StatusCode::kInvalidArgument));
}

TEST(BenchmarkConfigTest, InvalidQuerySize) {
  auto config =
      ParseArgs({"placeholder", "--project=test-project", "--query-size=0"});
  EXPECT_THAT(config, StatusIs(StatusCode::kInvalidArgument));
}

TEST(BenchmarkConfigTest, InvalidTableSize) {
  auto config = ParseArgs({"placeholder", "--project=test-project",
                           "--query-size=100", "--table-size=10"});
  EXPECT_THAT(config, StatusIs(StatusCode::kInvalidArgument));
}

TEST(BenchmarkConfigTest, OnlyStubsAndOnlyClients) {
  auto config = ParseArgs({"placeholder", "--project=test-project",
                           "--use-only-stubs", "--use-only-clients"});
  EXPECT_THAT(config, StatusIs(StatusCode::kInvalidArgument));
}

TEST(BenchmarkConfigTest, OnlyStubs) {
  auto config =
      ParseArgs({"placeholder", "--project=test-project", "--use-only-stubs"});
  ASSERT_STATUS_OK(config);

  EXPECT_TRUE(config->use_only_stubs);
}

TEST(BenchmarkConfigTest, OnlyClients) {
  auto config = ParseArgs(
      {"placeholder", "--project=test-project", "--use-only-clients"});
  ASSERT_STATUS_OK(config);

  EXPECT_TRUE(config->use_only_clients);
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_benchmarks
}  // namespace cloud
}  // namespace google
