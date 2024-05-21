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
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include <cctype>
#include <chrono>
#include <future>
#include <iomanip>
#include <sstream>

using ::google::cloud::bigquery_v2_minimal_benchmarks::Benchmark;
using ::google::cloud::bigquery_v2_minimal_benchmarks::BenchmarkResult;
using ::google::cloud::bigquery_v2_minimal_benchmarks::FormatDuration;
using ::google::cloud::bigquery_v2_minimal_benchmarks::JobBenchmark;
using ::google::cloud::bigquery_v2_minimal_benchmarks::JobConfig;
using ::google::cloud::bigquery_v2_minimal_benchmarks::OperationResult;

char const kDescription[] =
    R"""(Measures the latency of BigQuery's`CancelJob()` API.

This benchmark measures the latency of BigQuery's `CancelJob()` API.
The benchmark:
- Starts T threads as supplied in the command-line, executing the
  following loop:
- Runs for the test duration as supplied in the command-line, constantly
  executing this basic block:
  - Makes a rest call to `CancelJob()` API.
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

struct JobBenchmarkResult {
  BenchmarkResult cancel_job_results;
};

// Cancels an already running job.
OperationResult RunCancelJob(JobBenchmark& benchmark) {
  auto op = [&benchmark]() -> google::cloud::Status {
    return benchmark.CancelJob().status();
  };
  return Benchmark::TimeOperation(std::move(op));
}

// Run an iteration of the test.
google::cloud::StatusOr<JobBenchmarkResult> RunJobBenchmark(
    JobBenchmark& benchmark, absl::Duration test_duration) {
  JobBenchmarkResult result = {};
  std::uniform_int_distribution<int> prng_operation(0, 1);

  auto start = absl::Now();
  auto mark = start + test_duration / kBenchmarkProgressMarks;
  auto end = start + test_duration;
  auto local_time_zone = absl::LocalTimeZone();
  for (auto now = start; now < end; now = absl::Now()) {
    // Call CancelJob.
    auto op_result = RunCancelJob(benchmark);
    if (!op_result.status.ok()) {
      return op_result.status;
    }
    result.cancel_job_results.operations.emplace_back(op_result);

    if (now >= mark) {
      mark = now + test_duration / kBenchmarkProgressMarks;
      std::cout << "Start Time=" << absl::FormatTime(start, local_time_zone)
                << "\nCurrent Progress Mark="
                << absl::FormatTime(now, local_time_zone)
                << "\nNext Progress Mark="
                << absl::FormatTime(mark, local_time_zone)
                << "\nEnd Time=" << absl::FormatTime(end, local_time_zone)
                << "\nNumber of CancelJob operations performed thus far= "
                << result.cancel_job_results.operations.size() << "\n...\n"
                << std::flush;
    } else if (now > end) {
      std::cout << "\nStart Time=" << absl::FormatTime(start, local_time_zone)
                << "\nEnd Time=" << absl::FormatTime(end, local_time_zone)
                << "\nTotal Number of CancelJob operations= "
                << result.cancel_job_results.operations.size() << "\n...\n"
                << std::flush;
    }
  }
  return result;
}
}  // anonymous namespace

int main(int argc, char* argv[]) {
  JobConfig config;
  {
    std::vector<std::string> args{argv, argv + argc};
    auto c = config.ParseArgs(args);
    if (!c) {
      std::cerr << "Error parsing command-line arguments: " << c.status()
                << "\n"
                << std::flush;
      return 1;
    }
    config = *std::move(c);
  }

  if (config.ExitAfterParse()) {
    if (config.wants_description) {
      std::cout << kDescription << "\n" << std::flush;
    }
    if (config.wants_help) {
      std::cout << "The usage information for Job benchmark lists out all the "
                   "flags needed by all the APIs being benchmarked, namely: "
                   "GetQueryResults, "
                   "Query, Query, GetqueryResults and InsertJob."
                << "\n"
                << std::flush;
      config.PrintUsage();
    }
    std::cout << "Exiting..." << "\n" << std::flush;
    return 0;
  }
  std::cout << "# Job Benchmark STARTED For CancelJob() API with test "
               "duration as ["
            << config.test_duration.count() << "] seconds" << "\n"
            << std::flush;

  JobBenchmark benchmark(config);
  // Start the threads running the project benchmark test.
  auto latency_test_start = absl::Now();
  std::vector<std::future<google::cloud::StatusOr<JobBenchmarkResult>>> tasks;
  // If the user requests only one thread, use the current thread.
  auto launch_policy =
      config.thread_count == 1 ? std::launch::deferred : std::launch::async;
  for (int i = 0; i != config.thread_count; ++i) {
    tasks.emplace_back(std::async(launch_policy, RunJobBenchmark,
                                  std::ref(benchmark),
                                  absl::FromChrono(config.test_duration)));
  }

  // Wait for the threads and combine all the results.
  JobBenchmarkResult combined{};
  int count = 0;
  auto append = [](JobBenchmarkResult& destination,
                   JobBenchmarkResult const& source) {
    auto append_ops = [](BenchmarkResult& d, BenchmarkResult const& s) {
      d.operations.insert(d.operations.end(), s.operations.begin(),
                          s.operations.end());
    };
    append_ops(destination.cancel_job_results, source.cancel_job_results);
  };
  for (auto& future : tasks) {
    auto result = future.get();
    if (!result) {
      std::cerr << "Standard exception raised by task[" << count
                << "]: " << result.status() << "\n"
                << std::flush;
    } else {
      append(combined, *result);
    }
    ++count;
  }
  auto latency_test_elapsed =
      absl::ToChronoMilliseconds(absl::Now() - latency_test_start);
  combined.cancel_job_results.elapsed = latency_test_elapsed;
  std::cout << " DONE. Elapsed Test Duration="
            << FormatDuration(latency_test_elapsed) << "\n"
            << std::flush;

  Benchmark::PrintLatencyResult(std::cout, "Latency-Results", "CancelJob()",
                                combined.cancel_job_results);

  Benchmark::PrintThroughputResult(std::cout, "Throughput-Results",
                                   "CancelJob()", combined.cancel_job_results);
  std::cout << "# Job Benchmark ENDED" << "\n" << std::flush;

  return 0;
}
