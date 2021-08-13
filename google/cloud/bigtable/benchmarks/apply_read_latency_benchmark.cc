// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/benchmarks/benchmark.h"
#include "google/cloud/bigtable/benchmarks/random_mutation.h"
#include <cctype>
#include <chrono>
#include <future>
#include <iomanip>
#include <sstream>

/**
 * @file
 *
 * Measure the latency of `bigtable::Table::Apply()` and
 * `bigtable::Table::ReadRow()`.
 *
 * This benchmark measures the latency of `bigtable::Table::Apply()` and
 * `bigtable::Table::ReadRow()` on a "typical" table serving data.  The
 * benchmark:
 * - Creates a table with 10,000,000 rows, each row with a
 *   single column family.
 * - The column family contains 10 columns, each column filled with a random
 *   100 byte string.
 * - The name of the table starts with `perf`, followed by random characters.
 * - If there is a collision on the table name the benchmark aborts immediately.
 * - The benchmark populates the table during an initial phase.  The benchmark
 *   uses `BulkApply()` to populate the table, multiple threads to populate
 *   in parallel, and provides an initial split hint when creating the table.
 * - The benchmark reports the throughput of this bulk upload phase.
 *
 * After successfully uploading the initial data, the main phase of the
 * benchmark starts. During this phase the benchmark will:
 *
 * - The benchmark starts T threads, executing the following loop:
 * - Runs for S seconds, constantly executing this basic block:
 *   - Randomly, with 50% probability, pick if the next operation is an
 *     `Apply()` or a `ReadRow()`.
 *   - If the operation is a `ReadRow()` pick one of the 10,000,000 keys at
 *     random, with uniform probability, then perform the operation, record the
 *     latency and whether the operation was successful.
 *   - If the operation is an `Apply()`, pick new values for all the fields at
 *     random, then perform the operation, record the latency and whether the
 *     operation was successful.
 *
 * The test then waits for all the threads to finish and:
 *
 * - Collects the results from all the threads.
 * - Report the number of operations of each type, the total running time, and
 *   the effective throughput.
 * - Report the results, including p0 (minimum), p50, p90, p95, p99, p99.9, and
 *   p100 (maximum) latencies.
 * - Delete the table.
 * - Report the same results in CSV format to make analysis easier.
 *
 * Using a command-line parameter the benchmark can be configured to create a
 * local gRPC server that implements the Cloud Bigtable APIs used by the
 * benchmark.  If this parameter is not used the benchmark uses the default
 * configuration, that is, a production instance of Cloud Bigtable unless the
 * CLOUD_BIGTABLE_EMULATOR environment variable is set.
 */

/// Helper functions and types for the apply_read_latency_benchmark.
namespace {

namespace bigtable = ::google::cloud::bigtable;
using bigtable::benchmarks::Benchmark;
using bigtable::benchmarks::BenchmarkResult;
using bigtable::benchmarks::FormatDuration;
using bigtable::benchmarks::kColumnFamily;
using bigtable::benchmarks::kNumFields;
using bigtable::benchmarks::MakeBenchmarkSetup;
using bigtable::benchmarks::MakeRandomMutation;
using bigtable::benchmarks::OperationResult;

struct LatencyBenchmarkResult {
  BenchmarkResult apply_results;
  BenchmarkResult read_results;
};

/// Run an iteration of the test.
google::cloud::StatusOr<LatencyBenchmarkResult> RunBenchmark(
    bigtable::benchmarks::Benchmark& benchmark, std::string app_profile_id,
    std::string const& table_id, std::chrono::seconds test_duration);

//@{
/// @name Test constants.  Defined as requirements in the original bug (#189).
/// How many times does each thread report progress.
constexpr int kBenchmarkProgressMarks = 4;
//@}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  auto setup = MakeBenchmarkSetup("perf", argc, argv);
  if (!setup) {
    std::cerr << setup.status() << "\n";
    return -1;
  }

  Benchmark benchmark(*setup);

  // Create and populate the table for the benchmark.
  benchmark.CreateTable();
  auto populate_results = benchmark.PopulateTable();
  if (!populate_results) {
    std::cerr << populate_results.status() << "\n";
    return 1;
  }

  Benchmark::PrintThroughputResult(std::cout, "perf", "Upload",
                                   *populate_results);

  auto data_client = benchmark.MakeDataClient();
  // Start the threads running the latency test.
  std::cout << "Running Latency Benchmark " << std::flush;
  auto latency_test_start = std::chrono::steady_clock::now();
  std::vector<std::future<google::cloud::StatusOr<LatencyBenchmarkResult>>>
      tasks;
  for (int i = 0; i != setup->thread_count(); ++i) {
    auto launch_policy = std::launch::async;
    if (setup->thread_count() == 1) {
      // If the user requests only one thread, use the current thread.
      launch_policy = std::launch::deferred;
    }
    tasks.emplace_back(std::async(launch_policy, RunBenchmark,
                                  std::ref(benchmark), setup->app_profile_id(),
                                  setup->table_id(), setup->test_duration()));
  }

  // Wait for the threads and combine all the results.
  LatencyBenchmarkResult combined{};
  int count = 0;
  auto append = [](LatencyBenchmarkResult& destination,
                   LatencyBenchmarkResult const& source) {
    auto append_ops = [](BenchmarkResult& d, BenchmarkResult const& s) {
      d.row_count += s.row_count;
      d.operations.insert(d.operations.end(), s.operations.begin(),
                          s.operations.end());
    };
    append_ops(destination.apply_results, source.apply_results);
    append_ops(destination.read_results, source.read_results);
  };
  for (auto& future : tasks) {
    auto result = future.get();
    if (!result) {
      std::cerr << "Standard exception raised by task[" << count
                << "]: " << result.status() << "\n";
    } else {
      append(combined, *result);
    }
    ++count;
  }
  auto latency_test_elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - latency_test_start);
  combined.apply_results.elapsed = latency_test_elapsed;
  combined.read_results.elapsed = latency_test_elapsed;
  std::cout << " DONE. Elapsed=" << FormatDuration(latency_test_elapsed)
            << ", Ops=" << combined.apply_results.operations.size()
            << ", Rows=" << combined.apply_results.row_count << "\n";

  Benchmark::PrintLatencyResult(std::cout, "perf", "Apply()",
                                combined.apply_results);
  Benchmark::PrintLatencyResult(std::cout, "perf", "ReadRow()",
                                combined.read_results);

  std::cout << bigtable::benchmarks::Benchmark::ResultsCsvHeader() << "\n";
  benchmark.PrintResultCsv(std::cout, "perf", "BulkApply()", "Latency",
                           *populate_results);
  benchmark.PrintResultCsv(std::cout, "perf", "Apply()", "Latency",
                           combined.apply_results);
  benchmark.PrintResultCsv(std::cout, "perf", "ReadRow()", "Latency",
                           combined.read_results);

  benchmark.DeleteTable();

  return 0;
}

namespace {
OperationResult RunOneApply(bigtable::Table& table, std::string row_key,
                            std::mt19937_64& generator) {
  bigtable::SingleRowMutation mutation(std::move(row_key));
  for (int field = 0; field != kNumFields; ++field) {
    mutation.emplace_back(MakeRandomMutation(generator, field));
  }
  auto op = [&table, &mutation]() -> google::cloud::Status {
    return table.Apply(std::move(mutation));
  };

  return Benchmark::TimeOperation(std::move(op));
}

OperationResult RunOneReadRow(bigtable::Table& table, std::string row_key) {
  auto op = [&table, &row_key]() -> google::cloud::Status {
    return table
        .ReadRow(std::move(row_key), bigtable::Filter::ColumnRangeClosed(
                                         kColumnFamily, "field0", "field9"))
        .status();
  };
  return Benchmark::TimeOperation(std::move(op));
}

google::cloud::StatusOr<LatencyBenchmarkResult> RunBenchmark(
    bigtable::benchmarks::Benchmark& benchmark, std::string app_profile_id,
    std::string const& table_id, std::chrono::seconds test_duration) {
  LatencyBenchmarkResult result = {};

  auto data_client = benchmark.MakeDataClient();
  bigtable::Table table(std::move(data_client), std::move(app_profile_id),
                        table_id);

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  std::uniform_int_distribution<int> prng_operation(0, 1);

  auto start = std::chrono::steady_clock::now();
  auto mark = start + test_duration / kBenchmarkProgressMarks;
  auto end = start + test_duration;
  for (auto now = start; now < end; now = std::chrono::steady_clock::now()) {
    auto row_key = benchmark.MakeRandomKey(generator);

    if (prng_operation(generator) == 0) {
      auto op_result = RunOneApply(table, row_key, generator);
      if (!op_result.status.ok()) {
        return op_result.status;
      }
      result.apply_results.operations.emplace_back(op_result);
      ++result.apply_results.row_count;
    } else {
      auto op_result = RunOneReadRow(table, row_key);
      if (!op_result.status.ok()) {
        return op_result.status;
      }
      result.read_results.operations.emplace_back(op_result);
      ++result.read_results.row_count;
    }
    if (now >= mark) {
      std::cout << "." << std::flush;
      mark = now + test_duration / kBenchmarkProgressMarks;
    }
  }
  return result;
}

}  // anonymous namespace
