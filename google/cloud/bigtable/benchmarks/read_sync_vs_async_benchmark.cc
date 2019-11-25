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
#include <chrono>
#include <future>
#include <iomanip>
#include <sstream>

/**
 * @file
 *
 * Measure the effective throughput of `bigtable::Table::ReadRow()` and
 * `bigtable::Table::AsyncReadRow()`.
 *
 * This benchmark measures the effective throughput of dedicating N threads to
 * read single rows from Cloud Bigtable via the C++ client library. The test
 * creates N threads running `bigtable::Table::ReadRow()` requests, and a
 * separate N threads running `bigtable::Table::AsyncReadRow()` requests.
 * It runs these threads for S seconds and reports the total number of requests
 * on each approach.
 *
 * More specifically, the benchmark:
 *
 * - Creates a table with 10,000,000 rows, each row with a single column family.
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
 * - The benchmark starts N threads to test the throughput of
 *  `bigtable::Table::ReadRow()`, each thread executes the following loop for
 *   S seconds:
 * - Pick one of the 10,000,000 keys at random, with uniform probability, then
 *   perform the operation, record the latency and whether the operation was
 *   successful.
 *
 * - The benchmark starts N threads to run a `CompletionQueue` event loop.
 * - The test then picks K random keys, with uniform probability, then
 *   starts an asynchronous `bigtable::Table::AsyncReadRow()` with that key.
 * - When the asynchronous operation completes it captures the latency for the
 *   request.  If less than S seconds have elapsed since the beginning of the
 *   test it starts another asynchronous read.
 * - After S seconds the benchmark waits for any outstanding requests, and
 *   shuts down the completion queue threads.
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
namespace bigtable = google::cloud::bigtable;
using namespace bigtable::benchmarks;

/// Run an iteration of the test.
google::cloud::StatusOr<BenchmarkResult> RunSyncBenchmark(
    bigtable::benchmarks::Benchmark& benchmark, std::string app_profile_id,
    std::string const& table_id, std::chrono::seconds test_duration);

class AsyncBenchmark {
 public:
  AsyncBenchmark(bigtable::benchmarks::Benchmark& benchmark,
                 std::string const& app_profile_id, std::string const& table_id)
      : benchmark_(benchmark),
        table_(benchmark_.MakeDataClient(), app_profile_id, table_id),
        generator_(std::random_device{}()) {}

  ~AsyncBenchmark() {
    cq_.Shutdown();
    for (auto& t : cq_threads_) {
      t.join();
    }
  }

  void ActivateCompletionQueue();
  BenchmarkResult Run(std::chrono::seconds test_duration, int request_count);

 private:
  void RunOneAsyncReadRow();
  void OnReadRow(std::chrono::steady_clock::time_point request_start,
                 google::cloud::StatusOr<std::pair<bool, bigtable::Row>>);

  std::mutex mu_;
  std::condition_variable cv_;
  google::cloud::grpc_utils::CompletionQueue cq_;
  std::vector<std::thread> cq_threads_;
  bigtable::benchmarks::Benchmark& benchmark_;
  bigtable::Table table_;
  google::cloud::internal::DefaultPRNG generator_;
  int outstanding_requests_ = 0;
  BenchmarkResult results_;
  std::chrono::steady_clock::time_point deadline_;
};

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

  benchmark.PrintThroughputResult(std::cout, "perf", "Upload",
                                  *populate_results);

  google::cloud::grpc_utils::CompletionQueue cq;
  std::vector<std::thread> cq_threads;

  auto data_client = benchmark.MakeDataClient();
  // Start the threads running the latency test.
  std::cout << "# Running ReadRow/AsyncReadRow Throughput Benchmark "
            << std::flush;

  AsyncBenchmark async_benchmark(benchmark, setup->app_profile_id(),
                                 setup->table_id());
  // Start the benchmark threads.
  auto test_start = std::chrono::steady_clock::now();
  auto elapsed = [test_start] {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - test_start);
  };
  std::vector<std::future<google::cloud::StatusOr<BenchmarkResult>>> tasks;
  for (int i = 0; i != setup->thread_count(); ++i) {
    std::cout << '=' << std::flush;
    async_benchmark.ActivateCompletionQueue();
    tasks.emplace_back(std::async(std::launch::async, RunSyncBenchmark,
                                  std::ref(benchmark), setup->app_profile_id(),
                                  setup->table_id(), setup->test_duration()));
  }

  // Wait for the threads and combine all the results.
  auto async_results =
      async_benchmark.Run(setup->test_duration(),
                          setup->thread_count() * setup->parallel_requests());

  int count = 0;
  auto append_ops = [](BenchmarkResult& d, BenchmarkResult const& s) {
    d.row_count += s.row_count;
    d.operations.insert(d.operations.end(), s.operations.begin(),
                        s.operations.end());
  };

  BenchmarkResult sync_results;
  for (auto& future : tasks) {
    auto result = future.get();
    if (!result) {
      std::cerr << "Standard exception raised by task[" << count
                << "]: " << result.status() << "\n";
    } else {
      append_ops(sync_results, *result);
    }
    ++count;
  }
  sync_results.elapsed = elapsed();
  async_results.elapsed = elapsed();
  std::cout << " DONE. Elapsed=" << FormatDuration(sync_results.elapsed)
            << ", Ops=" << sync_results.operations.size()
            << ", Rows=" << sync_results.row_count << "\n";

  benchmark.PrintLatencyResult(std::cout, "perf", "AsyncReadRow()",
                               async_results);
  benchmark.PrintLatencyResult(std::cout, "perf", "ReadRow()", sync_results);

  std::cout << bigtable::benchmarks::Benchmark::ResultsCsvHeader() << "\n";
  benchmark.PrintResultCsv(std::cout, "perf", "BulkApply()", "Latency",
                           *populate_results);
  benchmark.PrintResultCsv(std::cout, "perf", "AsyncReadRow()", "Latency",
                           async_results);
  benchmark.PrintResultCsv(std::cout, "perf", "ReadRow()", "Latency",
                           sync_results);

  benchmark.DeleteTable();
  cq.Shutdown();
  for (auto& t : cq_threads) {
    t.join();
  }

  return 0;
}

namespace {

void AsyncBenchmark::ActivateCompletionQueue() {
  cq_threads_.push_back(std::thread([this] { cq_.Run(); }));
}

BenchmarkResult AsyncBenchmark::Run(std::chrono::seconds test_duration,
                                    int request_count) {
  results_ = BenchmarkResult{};
  deadline_ = std::chrono::steady_clock::now() + test_duration;

  for (int i = 0; i != request_count; ++i) {
    RunOneAsyncReadRow();
  }
  std::unique_lock<std::mutex> lk(mu_);
  cv_.wait(lk, [this] { return outstanding_requests_ == 0; });
  return std::move(results_);
}

void AsyncBenchmark::RunOneAsyncReadRow() {
  using google::cloud::future;
  using google::cloud::StatusOr;

  auto row_key = [this] {
    std::lock_guard<std::mutex> lk(mu_);
    ++outstanding_requests_;
    return benchmark_.MakeRandomKey(generator_);
  }();

  auto request_start = std::chrono::steady_clock::now();
  table_
      .AsyncReadRow(cq_, row_key,
                    bigtable::Filter::ColumnRangeClosed(kColumnFamily, "field0",
                                                        "field9"))
      .then([this, request_start](
                future<StatusOr<std::pair<bool, bigtable::Row>>> f) {
        OnReadRow(request_start, f.get());
      });
}

void AsyncBenchmark::OnReadRow(
    std::chrono::steady_clock::time_point request_start,
    google::cloud::StatusOr<std::pair<bool, bigtable::Row>> row) {
  auto now = std::chrono::steady_clock::now();
  auto usecs = std::chrono::duration_cast<std::chrono::microseconds>(
      now - request_start);

  std::unique_lock<std::mutex> lk(mu_);
  outstanding_requests_--;
  results_.operations.push_back({row.status(), usecs});
  ++results_.row_count;
  if (now < deadline_) {
    lk.unlock();
    RunOneAsyncReadRow();
    return;
  }
  if (outstanding_requests_ == 0) {
    cv_.notify_all();
  }
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

google::cloud::StatusOr<BenchmarkResult> RunSyncBenchmark(
    bigtable::benchmarks::Benchmark& benchmark, std::string app_profile_id,
    std::string const& table_id, std::chrono::seconds test_duration) {
  BenchmarkResult result = {};

  auto data_client = benchmark.MakeDataClient();
  bigtable::Table table(std::move(data_client), std::move(app_profile_id),
                        table_id);

  // Don't eat all the entropy to generate random keys.
  google::cloud::internal::DefaultPRNG generator(std::random_device{}());

  auto start = std::chrono::steady_clock::now();
  auto mark = start + test_duration / kBenchmarkProgressMarks;
  auto end = start + test_duration;
  for (auto now = start; now < end; now = std::chrono::steady_clock::now()) {
    auto row_key = benchmark.MakeRandomKey(generator);

    auto op_result = RunOneReadRow(table, row_key);
    result.operations.emplace_back(op_result);
    ++result.row_count;
    if (now >= mark) {
      std::cout << "." << std::flush;
      mark = now + test_duration / kBenchmarkProgressMarks;
    }
  }
  return result;
}

}  // anonymous namespace
