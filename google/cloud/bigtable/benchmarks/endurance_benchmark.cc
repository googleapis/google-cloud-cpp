// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/benchmarks/benchmark.h"
#include "google/cloud/bigtable/benchmarks/random_mutation.h"
#include <future>
#include <iomanip>
#include <sstream>

char const kDescription[] =
    R"""(Measure the latency of `Table::Apply()` and `Table::ReadRow()` on a
long running program.

This benchmark measures the latency of `Apply()` and `ReadRow()` on a program
running for many hours. The benchmark:
- Creates an empty table with a single column family.
- The column family contains 10 columns, each column filled with a random 100
  byte string.
- If there is a collision on the table name the benchmark aborts immediately.

After successfully creating the table, the main phase of the benchmark starts.
During this phase the benchmark:

- Starts T threads, executing the following loop:
- Runs for S seconds (typically hours), constantly executing this basic block:
  - Select a row at random, read it.
  - Select a row at random, read it.
  - Select a row at random, write to it.

The test then waits for all the threads to finish and reports effective
throughput.

Using a command-line parameter the benchmark can be configured to create a local
gRPC server that implements the Cloud Bigtable APIs used by the benchmark.  If
this parameter is not used the benchmark uses the default configuration, that
is, a production instance of Cloud Bigtable unless the CLOUD_BIGTABLE_EMULATOR
environment variable is set.
)""";

/// Helper functions and types for the apply_read_latency_benchmark.
namespace {
namespace bigtable = ::google::cloud::bigtable;
using bigtable::benchmarks::Benchmark;
using bigtable::benchmarks::BenchmarkResult;
using bigtable::benchmarks::FormatDuration;
using bigtable::benchmarks::kColumnFamily;
using bigtable::benchmarks::kNumFields;
using bigtable::benchmarks::MakeRandomMutation;
using bigtable::benchmarks::OperationResult;
using bigtable::benchmarks::ParseArgs;

/// Run an iteration of the test, returns the number of operations.
google::cloud::StatusOr<long> RunBenchmark(  // NOLINT(google-runtime-int)
    bigtable::benchmarks::Benchmark const& benchmark,
    std::chrono::seconds test_duration);

}  // anonymous namespace

int main(int argc, char* argv[]) {
  auto options = ParseArgs(argc, argv, kDescription);
  if (!options) {
    std::cerr << options.status() << "\n";
    return -1;
  }
  if (options->exit_after_parse) return 0;

  Benchmark benchmark(*options);
  // Create and populate the table for the benchmark.
  benchmark.CreateTable();

  // Start the threads running the latency test.
  std::cout << "# Running Endurance Benchmark:\n";
  auto latency_test_start = std::chrono::steady_clock::now();
  // NOLINTNEXTLINE(google-runtime-int)
  std::vector<std::future<google::cloud::StatusOr<long>>> tasks;
  for (int i = 0; i != options->thread_count; ++i) {
    auto launch_policy = std::launch::async;
    if (options->thread_count == 1) {
      // If the user requests only one thread, use the current thread.
      launch_policy = std::launch::deferred;
    }
    tasks.emplace_back(std::async(launch_policy, RunBenchmark,
                                  std::ref(benchmark), options->test_duration));
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
  auto throughput = 1000.0 * static_cast<double>(combined) /
                    static_cast<double>(elapsed.count());
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
    bigtable::benchmarks::Benchmark const& benchmark,
    std::chrono::seconds test_duration) {
  BenchmarkResult partial = {};

  auto table = benchmark.MakeTable();

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
