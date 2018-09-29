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
#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <sstream>

/**
 * @file
 *
 * Measure the throughput of `bigtable::Table::ReadRows()`.
 *
 * This benchmark measures the throughput of `bigtable::Table::ReadRows()` on a
 * "typical" table used for serving data.  The benchmark:
 * - Creates a table with 10,000,000 rows, each row with a single column family,
 *   but with 10 columns.
 * - The name of the table starts with `scant`, followed by random characters.
 * - If there is a collision on the table name the benchmark aborts immediately.
 * - The benchmark populates the table during an initial phase.  The benchmark
 *   uses `BulkApply()` to populate the table, multiple threads to populate
 *   in parallel, and provides an initial split hint when creating the table.
 * - The benchmark reports the throughput of this bulk upload phase.
 *
 * After successfully uploading the initial data, the main phase of the
 * benchmark starts. During this phase the benchmark will:
 *
 * - Execute the following block with different scan sizes:
 *   - Execute the following loop for S seconds:
 *     - Pick one of the 10,000,000 keys at random, with uniform probability.
 *     - Scan the number rows starting the the key selected above.
 *     - Go back and pick a new random key.
 *
 * The benchmark will report throughput in rows per second for each scans with
 * 100, 1,000 and 10,000 rows.
 *
 * Using a command-line parameter the benchmark can be configured to create a
 * local gRPC server that implements the Cloud Bigtable APIs used by the
 * benchmark.  If this parameter is not used, the benchmark uses the default
 * configuration, that is, a production instance of Cloud Bigtable unless the
 * CLOUD_BIGTABLE_EMULATOR environment variable is set.
 */

/// Helper functions and types for the scan_throughput_benchmark.
namespace {
namespace bigtable = google::cloud::bigtable;
using namespace bigtable::benchmarks;

constexpr int kScanSizes[] = {100, 1000, 10000};

/// Run an iteration of the test.
BenchmarkResult RunBenchmark(bigtable::benchmarks::Benchmark const& benchmark,
                             std::shared_ptr<bigtable::DataClient> data_client,
                             long table_size,
                             bigtable::AppProfileId app_profile_id,
                             std::string const& table_id, long scan_size,
                             std::chrono::seconds test_duration);
}  // anonymous namespace

int main(int argc, char* argv[]) try {
  bigtable::benchmarks::BenchmarkSetup setup("scant", argc, argv);

  Benchmark benchmark(setup);

  // Create and populate the table for the benchmark.
  benchmark.CreateTable();
  auto populate_results = benchmark.PopulateTable();
  benchmark.PrintThroughputResult(std::cout, "scant", "Upload",
                                  populate_results);

  auto data_client = benchmark.MakeDataClient();
  std::map<std::string, BenchmarkResult> results_by_size;
  for (auto scan_size : kScanSizes) {
    std::cout << "# Running benchmark [" << scan_size << "] " << std::flush;
    auto start = std::chrono::steady_clock::now();
    auto combined =
        RunBenchmark(benchmark, data_client, setup.table_size(),
                     bigtable::AppProfileId(setup.app_profile_id()),
                     setup.table_id(), scan_size, setup.test_duration());
    using std::chrono::duration_cast;
    combined.elapsed = duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    std::cout << " DONE. Elapsed=" << FormatDuration(combined.elapsed)
              << ", Ops=" << combined.operations.size()
              << ", Rows=" << combined.row_count << std::endl;
    auto op_name = "Scan(" + std::to_string(scan_size) + ")";
    benchmark.PrintLatencyResult(std::cout, "scant", op_name, combined);
    results_by_size[op_name] = std::move(combined);
  }

  std::cout << bigtable::benchmarks::Benchmark::ResultsCsvHeader() << std::endl;
  benchmark.PrintResultCsv(std::cout, "scant", "BulkApply()", "Latency",
                           populate_results);
  for (auto& kv : results_by_size) {
    benchmark.PrintResultCsv(std::cout, "scant", kv.first, "IterationTime",
                             kv.second);
  }

  benchmark.DeleteTable();

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << std::endl;
  return 1;
}

namespace {
BenchmarkResult RunBenchmark(bigtable::benchmarks::Benchmark const& benchmark,
                             std::shared_ptr<bigtable::DataClient> data_client,
                             long table_size,
                             bigtable::AppProfileId app_profile_id,
                             std::string const& table_id, long scan_size,
                             std::chrono::seconds test_duration) {
  BenchmarkResult result = {};

  bigtable::Table table(std::move(data_client), app_profile_id, table_id);

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  std::uniform_int_distribution<long> prng(0, table_size - scan_size - 1);

  auto test_start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() < test_start + test_duration) {
    auto range =
        bigtable::RowRange::StartingAt(benchmark.MakeKey(prng(generator)));

    long count = 0;
    auto op = [&count, &table, &scan_size, &range]() {
      auto reader =
          table.ReadRows(bigtable::RowSet(std::move(range)), scan_size,
                         bigtable::Filter::ColumnRangeClosed(
                             kColumnFamily, "field0", "field9"));
      count = std::distance(reader.begin(), reader.end());
    };
    result.operations.push_back(Benchmark::TimeOperation(op));
    result.row_count += count;
  }
  return result;
}

}  // anonymous namespace
