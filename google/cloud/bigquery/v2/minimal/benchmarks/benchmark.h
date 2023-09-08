// Copyright 2023 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_BENCHMARKS_BENCHMARK_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_BENCHMARKS_BENCHMARK_H

#include "google/cloud/bigquery/v2/minimal/benchmarks/benchmarks_config.h"
#include "google/cloud/bigquery/v2/minimal/internal/dataset_client.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"
#include "google/cloud/bigquery/v2/minimal/internal/project_client.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_client.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/status_or.h"
#include <chrono>
#include <deque>
#include <string>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_benchmarks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_internal::Dataset;
using ::google::cloud::bigquery_v2_minimal_internal::DatasetClient;
using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::Job;
using ::google::cloud::bigquery_v2_minimal_internal::JobClient;
using ::google::cloud::bigquery_v2_minimal_internal::ListFormatDataset;
using ::google::cloud::bigquery_v2_minimal_internal::ListFormatJob;
using ::google::cloud::bigquery_v2_minimal_internal::ListFormatTable;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::Project;
using ::google::cloud::bigquery_v2_minimal_internal::ProjectClient;
using ::google::cloud::bigquery_v2_minimal_internal::Table;
using ::google::cloud::bigquery_v2_minimal_internal::TableClient;

/// The result of a single operation.
struct OperationResult {
  google::cloud::Status status;
  std::chrono::microseconds latency;
};

struct BenchmarkResult {
  std::chrono::milliseconds elapsed;
  std::deque<OperationResult> operations;
};

/**
 * Common Class used by the readonly and mutating api benchmark programs.
 */
class Benchmark {
 public:
  explicit Benchmark() = default;
  ~Benchmark() = default;

  Benchmark(Benchmark const&) = delete;
  Benchmark& operator=(Benchmark const&) = delete;

  /**
   * Measure the time to compute an operation.
   */
  template <typename Operation>
  static OperationResult TimeOperation(Operation&& op) {
    auto start = std::chrono::steady_clock::now();
    auto status = op();
    using std::chrono::duration_cast;
    auto elapsed = duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start);
    return OperationResult{status, elapsed};
  }

  /**
   * Print the result of a throughput test in human readable form.
   */
  static void PrintThroughputResult(std::ostream& os,
                                    std::string const& test_name,
                                    std::string const& phase,
                                    BenchmarkResult const& result);

  /**
   * Print the result of a latency test in human readable form.
   */
  static void PrintLatencyResult(std::ostream& os, std::string const& test_name,
                                 std::string const& operation,
                                 BenchmarkResult& result);
};

class DatasetBenchmark : public Benchmark {
 public:
  explicit DatasetBenchmark(DatasetConfig config);
  ~DatasetBenchmark() = default;

  StatusOr<Dataset> GetDataset();
  StreamRange<ListFormatDataset> ListDatasets();

  DatasetConfig GetConfig() { return config_; }
  std::shared_ptr<DatasetClient> GetClient() { return dataset_client_; }

 private:
  DatasetConfig config_;
  std::shared_ptr<DatasetClient> dataset_client_;
};

class JobBenchmark : public Benchmark {
 public:
  explicit JobBenchmark(JobConfig config);
  ~JobBenchmark() = default;

  StatusOr<Job> GetJob();
  StreamRange<ListFormatJob> ListJobs();
  StatusOr<Job> InsertJob();
  StatusOr<PostQueryResults> Query();
  StatusOr<GetQueryResults> QueryResults();

  JobConfig GetConfig() { return config_; }
  std::shared_ptr<JobClient> GetClient() { return job_client_; }

 private:
  JobConfig config_;
  std::shared_ptr<JobClient> job_client_;
};

class TableBenchmark : public Benchmark {
 public:
  explicit TableBenchmark(TableConfig config);
  ~TableBenchmark() = default;

  StatusOr<Table> GetTable();
  StreamRange<ListFormatTable> ListTables();

  TableConfig GetConfig() { return config_; }
  std::shared_ptr<TableClient> GetClient() { return table_client_; }

 private:
  TableConfig config_;
  std::shared_ptr<TableClient> table_client_;
};

class ProjectBenchmark : public Benchmark {
 public:
  explicit ProjectBenchmark(Config config);
  ~ProjectBenchmark() = default;

  StreamRange<Project> ListProjects();

  Config GetConfig() { return config_; }
  std::shared_ptr<ProjectClient> GetClient() { return project_client_; }

 private:
  Config config_;
  std::shared_ptr<ProjectClient> project_client_;
};

/**
 * Helper class to pretty print durations.
 */
struct FormatDuration {
  template <typename Rep, typename Period>
  explicit FormatDuration(std::chrono::duration<Rep, Period> d)
      : ns(std::chrono::duration_cast<std::chrono::nanoseconds>(d)) {}
  std::chrono::nanoseconds ns;
};

/**
 * Pretty print an elapsed time.
 *
 * Reports benchmarks time in human readable terms.  This operator
 * streams a FormatDuration in hours, minutes, seconds and sub-seconds.  Any
 * component that is zero gets omitted, e.g. 1 hour exactly is printed as 1h.
 *
 * If the time is less than 1 second then the format uses millisecond or
 * microsecond resolution, as appropriate.
 *
 * @param os the destination stream.
 * @param duration the duration value.
 * @return the stream after printing.
 */
std::ostream& operator<<(std::ostream& os, FormatDuration duration);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_benchmarks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_BENCHMARKS_BENCHMARK_H
