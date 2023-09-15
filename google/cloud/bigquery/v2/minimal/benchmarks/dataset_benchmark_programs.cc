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
#include "google/cloud/bigquery/v2/minimal/benchmarks/benchmarks_config.h"
#include "google/cloud/internal/make_status.h"
#include <cctype>
#include <chrono>
#include <future>
#include <iomanip>
#include <sstream>

using ::google::cloud::StatusCode;
using ::google::cloud::bigquery_v2_minimal_benchmarks::Benchmark;
using ::google::cloud::bigquery_v2_minimal_benchmarks::BenchmarkResult;
using ::google::cloud::bigquery_v2_minimal_benchmarks::DatasetBenchmark;
using ::google::cloud::bigquery_v2_minimal_benchmarks::DatasetConfig;
using ::google::cloud::bigquery_v2_minimal_benchmarks::FormatDuration;
using ::google::cloud::bigquery_v2_minimal_benchmarks::OperationResult;

char const kDescription[] =
    R"""(Measures the latency of Bigquery's `GetDataset()` and
    `ListDatasets()` apis.

This benchmark measures the latency of Bigquery's `GetDataset()` and
    `ListDatasets()` apis.  The benchmark:
- Starts T threads as supplied in the command-line, executing the
  following loop:
- Runs for the test duration as supplied in the command-line, constantly
  executing this basic block:
  - Randomly, with 50% probability, makes a rest call to `GetDataset()`
    and `ListDatasets()` apis alternatively.
  - If either call fail, the test returns with the failure message.
  - Reports progress based on the total executing time and where the
    test is currently.

The test then waits for all the threads to finish and:

- Collects the results from all the threads.
- Reports the total running time.
- Reports the latency results, including p0 (minimum), p50, p90, p95, p99, p99.9, and
  p100 (maximum) latencies.
)""";

// Helper functions and types for dataset benchmark program.
namespace {
// Number of Progress report threads.
constexpr int kBenchmarkProgressMarks = 4;

struct DatasetBenchmarkResult {
  BenchmarkResult get_results;
  BenchmarkResult list_results;
};

// Gets a single dataset.
OperationResult RunGetDataset(DatasetBenchmark& benchmark) {
  auto op = [&benchmark]() -> google::cloud::Status {
    return benchmark.GetDataset().status();
  };
  return Benchmark::TimeOperation(std::move(op));
}

// Gets a lists of datasets.
OperationResult RunListDatasets(DatasetBenchmark& benchmark) {
  std::deque<OperationResult> operations;
  auto op = [&benchmark]() -> ::google::cloud::Status {
    auto datasets = benchmark.ListDatasets();
    for (auto& dataset : datasets) {
      auto op_status = dataset.status();
      if (!op_status.ok()) {
        return op_status;
      }
    }

    return ::google::cloud::Status(StatusCode::kOk, "");
  };
  return Benchmark::TimeOperation(std::move(op));
}

// Run an iteration of the test.
google::cloud::StatusOr<DatasetBenchmarkResult> RunDatasetBenchmark(
    DatasetBenchmark& benchmark, std::chrono::seconds test_duration) {
  DatasetBenchmarkResult result = {};
  auto generator = google::cloud::internal::MakeDefaultPRNG();
  std::uniform_int_distribution<int> prng_operation(0, 1);

  auto start = std::chrono::steady_clock::now();
  auto mark = start + test_duration / kBenchmarkProgressMarks;
  auto end = start + test_duration;
  for (auto now = start; now < end; now = std::chrono::steady_clock::now()) {
    if (prng_operation(generator) == 0) {
      // Call GetDataset.
      auto op_result = RunGetDataset(benchmark);
      if (!op_result.status.ok()) {
        return op_result.status;
      }
      result.get_results.operations.emplace_back(op_result);
    } else {
      // Call ListDatasets.
      auto op_result = RunListDatasets(benchmark);
      if (!op_result.status.ok()) {
        return op_result.status;
      }
      result.list_results.operations.emplace_back(op_result);
    }
    if (now >= mark) {
      mark = now + test_duration / kBenchmarkProgressMarks;
      std::cout << "Total Execution time=" << end.time_since_epoch().count()
                << " (seconds)"
                << ", Current Progress Mark=" << now.time_since_epoch().count()
                << " (seconds)"
                << ", Remaining Progress Marks="
                << mark.time_since_epoch().count() << "(seconds)..."
                << std::endl;
    }
  }
  return result;
}
}  // anonymous namespace

int main(int argc, char* argv[]) {
  DatasetConfig config;
  {
    std::vector<std::string> args{argv, argv + argc};
    auto c = config.ParseArgs(args);
    if (!c) {
      std::cerr << "Error parsing command-line arguments: " << c.status()
                << std::endl;
      return 1;
    }
    config = *std::move(c);
  }

  if (config.ExitAfterParse()) {
    if (config.wants_description) {
      std::cout << kDescription << std::endl;
    }
    if (config.wants_help) {
      config.PrintUsage();
    }
    std::cout << "Exiting..." << std::endl;
    return 0;
  }
  std::cout << "# Dataset Benchmark STARTED For GetDataset() and "
               "ListDatasets() apis with test duration as ["
            << config.test_duration.count() << "] seconds" << std::endl;

  DatasetBenchmark benchmark(config);
  // Start the threads running the dataset benchmark test.
  auto latency_test_start = std::chrono::steady_clock::now();
  std::vector<std::future<google::cloud::StatusOr<DatasetBenchmarkResult>>>
      tasks;
  for (int i = 0; i != config.thread_count; ++i) {
    auto launch_policy = std::launch::async;
    if (config.thread_count == 1) {
      // If the user requests only one thread, use the current thread.
      launch_policy = std::launch::deferred;
    }
    tasks.emplace_back(std::async(launch_policy, RunDatasetBenchmark,
                                  std::ref(benchmark), config.test_duration));
  }

  // Wait for the threads and combine all the results.
  DatasetBenchmarkResult combined{};
  int count = 0;
  auto append = [](DatasetBenchmarkResult& destination,
                   DatasetBenchmarkResult const& source) {
    auto append_ops = [](BenchmarkResult& d, BenchmarkResult const& s) {
      d.operations.insert(d.operations.end(), s.operations.begin(),
                          s.operations.end());
    };
    append_ops(destination.get_results, source.get_results);
    append_ops(destination.list_results, source.list_results);
  };
  for (auto& future : tasks) {
    auto result = future.get();
    if (!result) {
      std::cerr << "Standard exception raised by task[" << count
                << "]: " << result.status() << std::endl;
    } else {
      append(combined, *result);
    }
    ++count;
  }
  auto latency_test_elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - latency_test_start);
  combined.get_results.elapsed = latency_test_elapsed;
  combined.list_results.elapsed = latency_test_elapsed;
  std::cout << " DONE. Elapsed Test Duration="
            << FormatDuration(latency_test_elapsed) << std::endl;

  Benchmark::PrintLatencyResult(std::cout, "Latency-Results", "GetDataset()",
                                combined.get_results);
  Benchmark::PrintLatencyResult(std::cout, "Latency-Results", "ListDatasets()",
                                combined.list_results);
  std::cout << "# Dataset Benchmark ENDED" << std::endl;

  return 0;
}
