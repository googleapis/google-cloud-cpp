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

#include "google/cloud/bigquery/v2/minimal/benchmarks/benchmark.h"
#include "google/cloud/internal/build_info.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_benchmarks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_internal::DatasetClient;
using ::google::cloud::bigquery_v2_minimal_internal::JobClient;
using ::google::cloud::bigquery_v2_minimal_internal::ProjectClient;
using ::google::cloud::bigquery_v2_minimal_internal::TableClient;
using ::testing::HasSubstr;

TEST(DatasetBenchmarkTest, Create) {
  DatasetConfig c;
  auto config = c.ParseArgs(
      {"placeholder", "--endpoint=https://private.bigquery.googleapis.com",
       "--project=test-project", "--maximum-results=50",
       "--dataset=test-dataset", "--connection-pool-size=10",
       "--filter=labels.department:receiving", "--all=true"});
  EXPECT_STATUS_OK(config);

  {
    DatasetBenchmark bm(*config);

    EXPECT_EQ("https://private.bigquery.googleapis.com", config->endpoint);
    EXPECT_EQ("test-project", config->project_id);
    EXPECT_EQ(50, config->max_results);
    EXPECT_EQ(10, config->connection_pool_size);
    EXPECT_EQ(true, config->all);
    EXPECT_EQ("test-dataset", config->dataset_id);
    EXPECT_EQ("labels.department:receiving", config->filter);

    EXPECT_TRUE(bm.GetClient() != nullptr);
  }
  SUCCEED() << "DatasetBenchmark object successfully created";
}

TEST(TableBenchmarkTest, Create) {
  TableConfig c;
  auto config = c.ParseArgs(
      {"placeholder", "--endpoint=https://private.bigquery.googleapis.com",
       "--project=test-project", "--maximum-results=50",
       "--dataset=test-dataset", "--table=test-table",
       "--connection-pool-size=10", "--selected-fields=FIELD-1,FIELD-2",
       "--view=STORAGE_STATS"});
  EXPECT_STATUS_OK(config);

  {
    TableBenchmark bm(*config);

    EXPECT_EQ("https://private.bigquery.googleapis.com", config->endpoint);
    EXPECT_EQ("test-project", config->project_id);
    EXPECT_EQ(50, config->max_results);
    EXPECT_EQ(10, config->connection_pool_size);
    EXPECT_EQ("test-dataset", config->dataset_id);
    EXPECT_EQ("test-table", config->table_id);
    EXPECT_EQ("STORAGE_STATS", config->view.value);
    EXPECT_EQ("FIELD-1,FIELD-2", config->selected_fields);

    EXPECT_TRUE(bm.GetClient() != nullptr);
  }
  SUCCEED() << "TableBenchmark object successfully created";
}

TEST(ProjectBenchmarkTest, Create) {
  Config c;
  auto config = c.ParseArgs(
      {"placeholder", "--endpoint=https://private.bigquery.googleapis.com",
       "--project=test-project", "--maximum-results=50",
       "--connection-pool-size=10"});
  EXPECT_STATUS_OK(config);

  {
    ProjectBenchmark bm(*config);

    EXPECT_EQ("https://private.bigquery.googleapis.com", config->endpoint);
    EXPECT_EQ("test-project", config->project_id);
    EXPECT_EQ(50, config->max_results);
    EXPECT_EQ(10, config->connection_pool_size);

    EXPECT_TRUE(bm.GetClient() != nullptr);
  }
  SUCCEED() << "ProjectBenchmark object successfully created";
}

TEST(JobBenchmarkTest, Create) {
  JobConfig c;
  auto config = c.ParseArgs(
      {"placeholder", "--endpoint=https://private.bigquery.googleapis.com",
       "--project=test-project", "--maximum-results=50", "--job=1234",
       "--connection-pool-size=10", "--location=useast", "--parent-job-id=123",
       "--min-creation-time=12345678", "--max-creation-time=12345678",
       "--use-int64-timestamp=true", "--timeout-ms=12345", "--start-index=1",
       "--all-users=true", "--dry-run=true", "--query-create-replace=true",
       "--query-drop=true", "--projection=FULL", "--state-filter=DONE"});
  EXPECT_STATUS_OK(config);

  {
    JobBenchmark bm(*config);
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
    EXPECT_EQ(12345, config->timeout_ms);
    EXPECT_EQ(1, config->start_index);
    EXPECT_EQ("1234", config->job_id);
    EXPECT_EQ("useast", config->location);
    EXPECT_EQ("123", config->parent_job_id);
    EXPECT_EQ("12345678", config->min_creation_time);
    EXPECT_EQ("12345678", config->max_creation_time);

    EXPECT_TRUE(bm.GetClient() != nullptr);
  }
  SUCCEED() << "JobBenchmark object successfully created";
}

TEST(BenchmarkTest, PrintThroughputResult) {
  BenchmarkResult result{};
  result.elapsed = std::chrono::milliseconds(10000);
  result.operations.resize(3450);

  std::ostringstream os;
  Benchmark::PrintThroughputResult(os, "PrintThroughputResult", "Query",
                                   result);
  std::string output = os.str();

  // The output includes "XX ops/s" where XX is the operations count.
  EXPECT_THAT(output, HasSubstr("345 ops/s"));
  EXPECT_THAT(output, HasSubstr("Total elapsed time=10 seconds"));
  EXPECT_THAT(output, HasSubstr("Total number of operations performed=3450"));
}

TEST(BenchmarkTest, PrintLatencyResult) {
  BenchmarkResult result{};
  result.elapsed = std::chrono::milliseconds(1000);
  result.operations.resize(100);
  int count = 0;
  std::generate(result.operations.begin(), result.operations.end(), [&count]() {
    return OperationResult{google::cloud::Status(StatusCode::kOk, ""),
                           std::chrono::microseconds(++count * 100)};
  });

  std::ostringstream os;
  Benchmark::PrintLatencyResult(os, "PrintLatencyResult", "Query", result);
  std::string output = os.str();

  EXPECT_THAT(output, HasSubstr("Latency And Status: p0=100us"));
  EXPECT_THAT(output, HasSubstr("p50=5.1ms"));
  EXPECT_THAT(output, HasSubstr("p90=9ms"));
  EXPECT_THAT(output, HasSubstr("p95=9.5ms"));
  EXPECT_THAT(output, HasSubstr("p99=9.9ms"));
  EXPECT_THAT(output, HasSubstr("p100=10ms"));
  EXPECT_THAT(output, HasSubstr("status=OK"));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_benchmarks
}  // namespace cloud
}  // namespace google
