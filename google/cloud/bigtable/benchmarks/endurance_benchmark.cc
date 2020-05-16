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
#include <future>
#include <iomanip>
#include <sstream>

/**
 * @file
 *
 * Measure the latency of `bigtable::Table::Apply()` and
 * `bigtable::Table::ReadRow()` on a long running program.
 *
 * This benchmark measures the latency of `bigtable::Table::Apply()` and
 * `bigtable::Table::ReadRow()` on a program running for many hours. The
 * benchmark:
 * - Creates an empty table with a single column family.
 * - The column family contains 10 columns, each column filled with a random
 *   100 byte string.
 * - The name of the table starts with `long`, followed by random characters.
 * - If there is a collision on the table name the benchmark aborts immediately.
 *
 * After successfully creating the table, the main phase of the benchmark
 * starts. During this phase the benchmark:
 *
 * - Starts T threads, executing the following loop:
 * - Runs for S seconds (typically hours), constantly executing this
 * basic block:
 *   - Select a row at random, read it.
 *   - Select a row at random, read it.
 *   - Select a row at random, write to it.
 *
 * The test then waits for all the threads to finish and reports effective
 * throughput.
 *
 * Using a command-line parameter the benchmark can be configured to create a
 * local gRPC server that implements the Cloud Bigtable APIs used by the
 * benchmark.  If this parameter is not used the benchmark uses the default
 * configuration, that is, a production instance of Cloud Bigtable unless the
 * CLOUD_BIGTABLE_EMULATOR environment variable is set.
 */

/// Helper functions and types for the apply_read_latency_benchmark.
namespace {
namespace bigtable = google::cloud::bigtable;
using bigtable::benchmarks::Benchmark;
using bigtable::benchmarks::BenchmarkResult;
using bigtable::benchmarks::FormatDuration;
using bigtable::benchmarks::kColumnFamily;
using bigtable::benchmarks::kNumFields;
using bigtable::benchmarks::MakeBenchmarkSetup;
using bigtable::benchmarks::MakeRandomMutation;
using bigtable::benchmarks::OperationResult;

/// Run an iteration of the test, returns the number of operations.
google::cloud::StatusOr<long> RunBenchmark(  // NOLINT(google-runtime-int)
    bigtable::benchmarks::Benchmark& benchmark, std::string app_profile_id,
    std::string const& table_id, std::chrono::seconds test_duration);

}  // anonymous namespace

int main(int argc, char* argv[]) {
  auto setup = bigtable::benchmarks::MakeBenchmarkSetup("long", argc, argv);
  if (!setup) {
    std::cerr << setup.status() << "\n";
    return -1;
  }

  Benchmark benchmark(*setup);
  // Create and populate the table for the benchmark.
  benchmark.CreateTable();

  // Start the threads running the latency test.
  std::cout << "# Running Endurance Benchmark:\n";
  auto latency_test_start = std::chrono::steady_clock::now();
  // NOLINTNEXTLINE(google-runtime-int)
  std::vector<std::future<google::cloud::StatusOr<long>>> tasks;
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
  long combined = 0;  // NOLINT(google-runtime-int)
  int count = 0;
  for (auto& future : tasks) {
    auto result = future.get();
    if (!result) {
      std::cerr << "Error returned by task[" << count
                << "]: " << result.status() << "\n";
    } else {
      combined += *result;
    }
    ++count;
  }
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - latency_test_start);
  auto throughput = 1000.0 * static_cast<double>(combined) / elapsed.count();
  std::cout << "# DONE. Elapsed=" << FormatDuration(elapsed)
            << ", Ops=" << combined << ", Throughput: " << throughput
            << " ops/sec\n";

  benchmark.DeleteTable();
  return 0;
}

namespace {

OperationResult RunOneApply(bigtable::Table& table, Benchmark const& benchmark,
                            google::cloud::internal::DefaultPRNG& generator) {
  auto row_key = benchmark.MakeRandomKey(generator);
  bigtable::SingleRowMutation mutation(std::move(row_key));
  for (int field = 0; field != kNumFields; ++field) {
    mutation.emplace_back(MakeRandomMutation(generator, field));
  }
  auto op = [&table, &mutation]() -> google::cloud::Status {
    return table.Apply(std::move(mutation));
  };
  return Benchmark::TimeOperation(std::move(op));
}

OperationResult RunOneReadRow(bigtable::Table& table,
                              Benchmark const& benchmark,
                              google::cloud::internal::DefaultPRNG& generator) {
  auto row_key = benchmark.MakeRandomKey(generator);
  auto op = [&table, &row_key]() -> google::cloud::Status {
    return table
        .ReadRow(std::move(row_key), bigtable::Filter::ColumnRangeClosed(
                                         kColumnFamily, "field0", "field9"))
        .status();
  };
  return Benchmark::TimeOperation(std::move(op));
}

google::cloud::StatusOr<long> RunBenchmark(  // NOLINT(google-runtime-int)
    bigtable::benchmarks::Benchmark& benchmark, std::string app_profile_id,
    std::string const& table_id, std::chrono::seconds test_duration) {
  BenchmarkResult partial = {};

  auto data_client = benchmark.MakeDataClient();
  bigtable::Table table(std::move(data_client), std::move(app_profile_id),
                        table_id);

  auto generator = google::cloud::internal::MakeDefaultPRNG();

  auto start = std::chrono::steady_clock::now();
  auto end = start + test_duration;

  for (auto now = start; now < end; now = std::chrono::steady_clock::now()) {
    auto op_result = RunOneReadRow(table, benchmark, generator);
    if (!op_result.status.ok()) {
      return op_result.status;
    }
    partial.operations.emplace_back(op_result);
    ++partial.row_count;
    op_result = RunOneReadRow(table, benchmark, generator);
    if (!op_result.status.ok()) {
      return op_result.status;
    }
    partial.operations.emplace_back(op_result);
    ++partial.row_count;
    op_result = RunOneApply(table, benchmark, generator);
    if (!op_result.status.ok()) {
      return op_result.status;
    }
    partial.operations.emplace_back(op_result);
    ++partial.row_count;
  }
  partial.elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  std::ostringstream msg;
  Benchmark::PrintLatencyResult(msg, "long", "Partial::Op", partial);
  std::cout << msg.str() << std::flush;
  // NOLINTNEXTLINE(google-runtime-int)
  return static_cast<long>(partial.operations.size());
}

}  // anonymous namespace
