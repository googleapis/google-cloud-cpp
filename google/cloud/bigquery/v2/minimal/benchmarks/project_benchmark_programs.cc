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
#include "absl/time/time.h"
#include <cctype>
#include <chrono>
#include <future>
#include <iomanip>
#include <sstream>

using ::google::cloud::StatusCode;
using ::google::cloud::bigquery_v2_minimal_benchmarks::Benchmark;
using ::google::cloud::bigquery_v2_minimal_benchmarks::BenchmarkResult;
using ::google::cloud::bigquery_v2_minimal_benchmarks::Config;
using ::google::cloud::bigquery_v2_minimal_benchmarks::FormatDuration;
using ::google::cloud::bigquery_v2_minimal_benchmarks::OperationResult;
using ::google::cloud::bigquery_v2_minimal_benchmarks::ProjectBenchmark;
using std::chrono::system_clock;

char const kDescription[] =
    R"""(Measures the latency of Bigquery's`ListProjects()` api.

This benchmark measures the latency of Bigquery's `ListProjects()` api.
The benchmark:
- Starts T threads as supplied in the command-line, executing the
  following loop:
- Runs for the test duration as supplied in the command-line, constantly
  executing this basic block:
  - Makes a rest call to `ListProjects()` api.
  - If the call fails, the test returns with the failure message.
  - Reports progress based on the total executing time and where the
    test is currently.

The test then waits for all the threads to finish and:

- Collects the results from all the threads.
- Reports the total running time.
- Reports the latency results, including p0 (minimum), p50, p90, p95, p99, p99.9, and
  p100 (maximum) latencies.
)""";

// Helper functions and types for project benchmark program.
namespace {
// Number of Progress report threads.
constexpr int kBenchmarkProgressMarks = 4;

struct ProjectBenchmarkResult {
  BenchmarkResult list_results;
};

// Gets a lists of projects.
OperationResult RunListProjects(ProjectBenchmark& benchmark) {
  std::deque<OperationResult> operations;
  auto op = [&benchmark]() -> ::google::cloud::Status {
    auto projects = benchmark.ListProjects();
    int project_count = 0;
    for (auto& project : projects) {
      auto op_status = project.status();
      if (!op_status.ok()) {
        return op_status;
      }
      project_count++;
    }

    std::cout << "#"
              << " ListProjects(): Total Items fetched: " << project_count
              << std::endl;

    return ::google::cloud::Status(StatusCode::kOk, "");
  };
  return Benchmark::TimeOperation(std::move(op));
}

// Run an iteration of the test.
google::cloud::StatusOr<ProjectBenchmarkResult> RunProjectBenchmark(
    ProjectBenchmark& benchmark, std::chrono::seconds test_duration) {
  ProjectBenchmarkResult result = {};
  std::uniform_int_distribution<int> prng_operation(0, 1);

  auto start = system_clock::now();
  auto mark = start + test_duration / kBenchmarkProgressMarks;
  auto end = start + test_duration;
  for (auto now = start; now < end; now = system_clock::now()) {
    // Call ListProjects.
    auto op_result = RunListProjects(benchmark);
    if (!op_result.status.ok()) {
      return op_result.status;
    }
    result.list_results.operations.emplace_back(op_result);

    std::time_t start_t = system_clock::to_time_t(start);
    auto start_time = absl::FromTimeT(start_t);
    std::time_t end_t = system_clock::to_time_t(end);
    auto end_time = absl::FromTimeT(end_t);
    std::time_t now_t = system_clock::to_time_t(now);
    auto now_time = absl::FromTimeT(now_t);
    if (now >= mark) {
      mark = now + test_duration / kBenchmarkProgressMarks;
      std::time_t mark_t = system_clock::to_time_t(mark);
      auto mark_time = absl::FromTimeT(mark_t);
      std::cout << "Start Time="
                << absl::FormatTime(start_time, absl::LocalTimeZone())
                << std::endl
                << "Current Progress Mark="
                << absl::FormatTime(now_time, absl::LocalTimeZone())
                << std::endl
                << "Next Progress Mark="
                << absl::FormatTime(mark_time, absl::LocalTimeZone())
                << std::endl
                << "End Time="
                << absl::FormatTime(end_time, absl::LocalTimeZone())
                << std::endl
                << ", Number of ListProjects operations performed thus far= "
                << result.list_results.operations.size() << std::endl;
      std::cout << "..." << std::endl;
    } else if (now > end) {
      std::cout << "Start Time="
                << absl::FormatTime(start_time, absl::LocalTimeZone())
                << std::endl
                << "End Time="
                << absl::FormatTime(end_time, absl::LocalTimeZone())
                << std::endl
                << ", Total Number of ListProjects operations= "
                << result.list_results.operations.size() << std::endl;
      std::cout << "..." << std::endl;
    }
  }
  return result;
}
}  // anonymous namespace

int main(int argc, char* argv[]) {
  Config config;
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
  std::cout << "# Project Benchmark STARTED For ListProjects() api with test "
               "duration as ["
            << config.test_duration.count() << "] seconds" << std::endl;

  ProjectBenchmark benchmark(config);
  // Start the threads running the project benchmark test.
  auto latency_test_start = system_clock::now();
  std::vector<std::future<google::cloud::StatusOr<ProjectBenchmarkResult>>>
      tasks;
  // If the user requests only one thread, use the current thread.
  auto launch_policy =
      config.thread_count == 1 ? std::launch::deferred : std::launch::async;
  for (int i = 0; i != config.thread_count; ++i) {
    tasks.emplace_back(std::async(launch_policy, RunProjectBenchmark,
                                  std::ref(benchmark), config.test_duration));
  }

  // Wait for the threads and combine all the results.
  ProjectBenchmarkResult combined{};
  int count = 0;
  auto append = [](ProjectBenchmarkResult& destination,
                   ProjectBenchmarkResult const& source) {
    auto append_ops = [](BenchmarkResult& d, BenchmarkResult const& s) {
      d.operations.insert(d.operations.end(), s.operations.begin(),
                          s.operations.end());
    };
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
          system_clock::now() - latency_test_start);
  combined.list_results.elapsed = latency_test_elapsed;
  std::cout << " DONE. Elapsed Test Duration="
            << FormatDuration(latency_test_elapsed) << std::endl;

  Benchmark::PrintLatencyResult(std::cout, "Latency-Results", "ListProjects()",
                                combined.list_results);

  Benchmark::PrintThroughputResult(std::cout, "Throughput-Results",
                                   "ListProjects()", combined.list_results);
  std::cout << "# Project Benchmark ENDED" << std::endl;

  return 0;
}
