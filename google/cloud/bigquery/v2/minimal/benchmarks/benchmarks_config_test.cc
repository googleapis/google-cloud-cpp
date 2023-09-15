// Copyright 2023 Google LLC
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

#include "google/cloud/bigquery/v2/minimal/benchmarks/benchmarks_config.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_benchmarks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::testing_util::StatusIs;

TEST(BenchmarkConfigTest, ParseAllWithDescriptionAndHelp) {
  Config c;
  auto config = c.ParseArgs(
      {"placeholder", "--endpoint=https://private.bigquery.googleapis.com",
       "--wants-description=true", "--help=true", "--project=test-project",
       "--maximum-results=50", "--thread-count=10", "--test-duration=10",
       "--connection-pool-size=10"});
  EXPECT_STATUS_OK(config);

  EXPECT_EQ(true, config->wants_description);
  EXPECT_EQ(true, config->wants_help);
  EXPECT_EQ(true, config->ExitAfterParse());
}

TEST(BenchmarkConfigTest, ParseAllWithoutDescriptionAndHelp) {
  Config c;
  auto config = c.ParseArgs(
      {"placeholder", "--endpoint=https://private.bigquery.googleapis.com",
       "--project=test-project", "--maximum-results=50", "--thread-count=10",
       "--test-duration=10", "--connection-pool-size=10"});
  EXPECT_STATUS_OK(config);

  EXPECT_EQ("https://private.bigquery.googleapis.com", config->endpoint);
  EXPECT_EQ("test-project", config->project_id);
  EXPECT_EQ(50, config->max_results);
  EXPECT_EQ(10, config->thread_count);
  EXPECT_EQ(10, config->connection_pool_size);
  EXPECT_EQ(10, config->test_duration.count());
  EXPECT_EQ(false, config->wants_description);
  EXPECT_EQ(false, config->ExitAfterParse());
  EXPECT_EQ(false, config->wants_help);
}

TEST(BenchmarkConfigTest, ParseNone) {
  Config c;
  testing_util::ScopedEnvironment env("GOOGLE_CLOUD_PROJECT", "test-project");
  auto config = c.ParseArgs({"placeholder"});
  EXPECT_STATUS_OK(config);
  EXPECT_EQ("https://bigquery.googleapis.com", config->endpoint);
}

TEST(BenchmarkConfigTest, ParseWantsDescription) {
  Config c;
  auto config = c.ParseArgs({"placeholder", "--wants-description=true"});
  ASSERT_STATUS_OK(config);
  EXPECT_EQ(true, config->wants_description);
  EXPECT_EQ(true, config->ExitAfterParse());
  EXPECT_EQ(false, config->wants_help);
}

TEST(BenchmarkConfigTest, ParseWantsHelp) {
  Config c;
  auto config = c.ParseArgs({"placeholder", "--help=true"});
  ASSERT_STATUS_OK(config);
  EXPECT_EQ(true, config->wants_help);
  EXPECT_EQ(true, config->ExitAfterParse());
  EXPECT_EQ(false, config->wants_description);
}

TEST(BenchmarkConfigTest, ParseMaxResults) {
  Config c;
  auto config = c.ParseArgs(
      {"placeholder", "--project=test-project", "--maximum-results=4"});
  ASSERT_STATUS_OK(config);
  EXPECT_EQ(4, config->max_results);
}

TEST(BenchmarkConfigTest, ParseThreads) {
  Config c;
  auto config = c.ParseArgs(
      {"placeholder", "--project=test-project", "--thread-count=5"});
  ASSERT_STATUS_OK(config);
  EXPECT_EQ(5, config->thread_count);
}

TEST(BenchmarkConfigTest, ParseTestDuration) {
  Config c;
  auto config = c.ParseArgs(
      {"placeholder", "--project=test-project", "--test-duration=15"});
  ASSERT_STATUS_OK(config);
  EXPECT_EQ(15, config->test_duration.count());
}

TEST(BenchmarkConfigTest, ParseConnectionPoolSize) {
  Config c;
  auto config = c.ParseArgs(
      {"placeholder", "--project=test-project", "--connection-pool-size=4"});
  ASSERT_STATUS_OK(config);
  EXPECT_EQ(4, config->connection_pool_size);
}

TEST(BenchmarkConfigTest, InvalidFlag) {
  Config c;
  auto config = c.ParseArgs({"placeholder", "--not-a-flag=1"});
  EXPECT_THAT(config, StatusIs(StatusCode::kInvalidArgument));
}

TEST(BenchmarkConfigTest, EmptyEndpoint) {
  Config c;
  auto config =
      c.ParseArgs({"placeholder", "--project=test-project", "--endpoint="});
  EXPECT_THAT(config, StatusIs(StatusCode::kInvalidArgument));
}

TEST(BenchmarkConfigTest, EmptyProject) {
  Config c;
  auto config = c.ParseArgs({"placeholder", "--project="});
  EXPECT_THAT(config, StatusIs(StatusCode::kInvalidArgument));
}

TEST(BenchmarkConfigTest, InvalidMaximumResults) {
  Config c;
  auto config = c.ParseArgs(
      {"placeholder", "--project=test-project", "--maximum-results=-7"});
  EXPECT_THAT(config, StatusIs(StatusCode::kInvalidArgument));
}

TEST(BenchmarkConfigTest, InvalidConnectionPoolSize) {
  Config c;
  auto config = c.ParseArgs(
      {"placeholder", "--project=test-project", "--connection-pool-size=-7"});
  EXPECT_THAT(config, StatusIs(StatusCode::kInvalidArgument));
}

TEST(BenchmarkDatasetConfigTest, ParseAll) {
  DatasetConfig c;
  auto config = c.ParseArgs(
      {"placeholder", "--endpoint=https://private.bigquery.googleapis.com",
       "--project=test-project", "--maximum-results=50",
       "--dataset=test-dataset", "--connection-pool-size=10",
       "--filter=labels.department:receiving", "--all=true"});
  EXPECT_STATUS_OK(config);

  EXPECT_EQ("https://private.bigquery.googleapis.com", config->endpoint);
  EXPECT_EQ("test-project", config->project_id);
  EXPECT_EQ(50, config->max_results);
  EXPECT_EQ(10, config->connection_pool_size);
  EXPECT_EQ(true, config->all);
  EXPECT_EQ("test-dataset", config->dataset_id);
  EXPECT_EQ("labels.department:receiving", config->filter);
}

TEST(BenchmarkDatasetConfigTest, EmptyDataset) {
  DatasetConfig c;
  auto config =
      c.ParseArgs({"placeholder", "--project=test-project",
                   "--endpoint=https://private.bigquery.googleapis.com"});
  EXPECT_STATUS_OK(config);
}

TEST(BenchmarkTableConfigTest, ParseAll) {
  TableConfig c;
  auto config = c.ParseArgs(
      {"placeholder", "--endpoint=https://private.bigquery.googleapis.com",
       "--project=test-project", "--maximum-results=50",
       "--dataset=test-dataset", "--table=test-table",
       "--connection-pool-size=10", "--selected-fields=FIELD-1,FIELD-2",
       "--view=STORAGE_STATS"});
  EXPECT_STATUS_OK(config);

  EXPECT_EQ("https://private.bigquery.googleapis.com", config->endpoint);
  EXPECT_EQ("test-project", config->project_id);
  EXPECT_EQ(50, config->max_results);
  EXPECT_EQ(10, config->connection_pool_size);
  EXPECT_EQ("test-dataset", config->dataset_id);
  EXPECT_EQ("test-table", config->table_id);
  EXPECT_EQ("STORAGE_STATS", config->view.value);
  EXPECT_EQ("FIELD-1,FIELD-2", config->selected_fields);
}

TEST(BenchmarkTableConfigTest, EmptyDataset) {
  TableConfig c;
  auto config =
      c.ParseArgs({"placeholder", "--project=test-project",
                   "--endpoint=https://private.bigquery.googleapis.com"});
  EXPECT_THAT(config, StatusIs(StatusCode::kInvalidArgument));
}

TEST(BenchmarkJobConfigTest, ParseAll) {
  JobConfig c;
  auto config = c.ParseArgs(
      {"placeholder", "--endpoint=https://private.bigquery.googleapis.com",
       "--project=test-project", "--maximum-results=50", "--job=1234",
       "--connection-pool-size=10", "--location=useast", "--parent-job-id=123",
       "--min-creation-time=12345678", "--max-creation-time=12345678",
       "--all-users=true", "--dry-run=true", "--query-create-replace=true",
       "--use-int64-timestamp=true", "--timeout-ms=12345", "--start-index=1",
       "--query-drop=true", "--projection=FULL", "--state-filter=DONE"});
  EXPECT_STATUS_OK(config);

  EXPECT_EQ("https://private.bigquery.googleapis.com", config->endpoint);
  EXPECT_EQ("test-project", config->project_id);
  EXPECT_EQ(50, config->max_results);
  EXPECT_EQ(10, config->connection_pool_size);
  EXPECT_EQ("FULL", config->projection.value);
  EXPECT_EQ("DONE", config->state_filter.value);
  EXPECT_EQ(true, config->all_users);
  EXPECT_EQ(true, config->dry_run);
  EXPECT_EQ(true, config->query_create_replace);
  EXPECT_EQ(true, config->query_drop);
  EXPECT_EQ(true, config->use_int64_timestamp);
  EXPECT_EQ("1234", config->job_id);
  EXPECT_EQ(12345, config->timeout_ms);
  EXPECT_EQ(1, config->start_index);
  EXPECT_EQ("useast", config->location);
  EXPECT_EQ("123", config->parent_job_id);
  EXPECT_EQ("12345678", config->min_creation_time);
  EXPECT_EQ("12345678", config->max_creation_time);
}

TEST(BenchmarkJobConfigTest, EmptyJob) {
  JobConfig c;
  auto config =
      c.ParseArgs({"placeholder", "--project=test-project",
                   "--endpoint=https://private.bigquery.googleapis.com"});
  EXPECT_STATUS_OK(config);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_benchmarks
}  // namespace cloud
}  // namespace google
