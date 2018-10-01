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
using namespace bigtable::benchmarks;

/// Run an iteration of the test, returns the number of operations.
long RunBenchmark(bigtable::benchmarks::Benchmark& benchmark,
                  bigtable::AppProfileId app_profile_id,
                  std::string const& table_id,
                  std::chrono::seconds test_duration);

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  bigtable::benchmarks::BenchmarkSetup setup("long", argc, argv);
  Benchmark benchmark(setup);
  // Create and populate the table for the benchmark.
  benchmark.CreateTable();

  // Start the threads running the latency test.
  std::cout << "# Running Endurance Benchmark:" << std::endl;
  auto latency_test_start = std::chrono::steady_clock::now();
  std::vector<std::future<long>> tasks;
  for (int i = 0; i != setup.thread_count(); ++i) {
    auto launch_policy = std::launch::async;
    if (setup.thread_count() == 1) {
      // If the user requests only one thread, use the current thread.
      launch_policy = std::launch::deferred;
    }
    tasks.emplace_back(
        std::async(launch_policy, RunBenchmark, std::ref(benchmark),
                   bigtable::AppProfileId(setup.app_profile_id()),
                   setup.table_id(), setup.test_duration()));
  }

  // Wait for the threads and combine all the results.
  long combined = 0;
  int count = 0;
  for (auto& future : tasks) {
    try {
      auto result = future.get();
      combined += result;
    } catch (std::exception const& ex) {
      std::cerr << "Standard exception raised by task[" << count
                << "]: " << ex.what() << std::endl;
    }
    ++count;
  }
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - latency_test_start);
  auto throughput = 1000.0 * combined / elapsed.count();
  std::cout << "# DONE. Elapsed=" << FormatDuration(elapsed)
            << ", Ops=" << combined << ", Throughput: " << throughput
            << " ops/sec" << std::endl;

  benchmark.DeleteTable();
  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << std::endl;
  return 1;
}

namespace {
OperationResult RunOneApply(bigtable::Table& table, Benchmark const& benchmark,
                            google::cloud::internal::DefaultPRNG& generator) {
  auto row_key = benchmark.MakeRandomKey(generator);
  bigtable::SingleRowMutation mutation(std::move(row_key));
  for (int field = 0; field != kNumFields; ++field) {
    mutation.emplace_back(MakeRandomMutation(generator, field));
  }
  auto op = [&table, &mutation]() { table.Apply(std::move(mutation)); };
  return Benchmark::TimeOperation(std::move(op));
}

OperationResult RunOneReadRow(bigtable::Table& table,
                              Benchmark const& benchmark,
                              google::cloud::internal::DefaultPRNG& generator) {
  auto row_key = benchmark.MakeRandomKey(generator);
  auto op = [&table, &row_key]() {
    auto row = table.ReadRow(
        std::move(row_key),
        bigtable::Filter::ColumnRangeClosed(kColumnFamily, "field0", "field9"));
  };
  return Benchmark::TimeOperation(std::move(op));
}

long RunBenchmark(bigtable::benchmarks::Benchmark& benchmark,
                  bigtable::AppProfileId app_profile_id,
                  std::string const& table_id,
                  std::chrono::seconds test_duration) {
  long total_ops = 0;

  BenchmarkResult partial = {};

  auto data_client = benchmark.MakeDataClient();
  bigtable::Table table(std::move(data_client), app_profile_id, table_id);

  auto generator = google::cloud::internal::MakeDefaultPRNG();

  auto start = std::chrono::steady_clock::now();
  auto end = start + test_duration;

  for (auto now = start; now < end; now = std::chrono::steady_clock::now()) {
    partial.operations.emplace_back(RunOneReadRow(table, benchmark, generator));
    ++partial.row_count;
    partial.operations.emplace_back(RunOneReadRow(table, benchmark, generator));
    ++partial.row_count;
    partial.operations.emplace_back(RunOneApply(table, benchmark, generator));
    ++partial.row_count;
  }
  partial.elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  std::ostringstream msg;
  benchmark.PrintLatencyResult(msg, "long", "Partial::Op", partial);
  std::cout << msg.str() << std::flush;
  total_ops = partial.operations.size();
  return total_ops;
}

}  // anonymous namespace
