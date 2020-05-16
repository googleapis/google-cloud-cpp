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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_BENCHMARK_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_BENCHMARK_H

#include "google/cloud/bigtable/benchmarks/embedded_server.h"
#include "google/cloud/bigtable/benchmarks/setup.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/status_or.h"
#include <chrono>
#include <deque>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
namespace benchmarks {
/// The result of a single operation.
struct OperationResult {
  google::cloud::Status status;
  std::chrono::microseconds latency;
};

struct BenchmarkResult {
  std::chrono::milliseconds elapsed;
  std::deque<OperationResult> operations;
  long row_count;  // NOLINT(google-runtime-int)
};

/**
 * Common code used by the Cloud Bigtable C++ Client benchmarks.
 */
class Benchmark {
 public:
  explicit Benchmark(BenchmarkSetup setup);
  ~Benchmark();

  Benchmark(Benchmark const&) = delete;
  Benchmark& operator=(Benchmark const&) = delete;

  /// Create a table for the benchmark, return the table_id.
  std::string CreateTable();

  /// Delete the table used in the benchmark.
  void DeleteTable();

  /// Populate the table with initial data.
  google::cloud::StatusOr<BenchmarkResult> PopulateTable();

  /// Return a `bigtable::DataClient` configured for this benchmark.
  std::shared_ptr<bigtable::DataClient> MakeDataClient();

  /// Create a random key.
  std::string MakeRandomKey(google::cloud::internal::DefaultPRNG& gen) const;

  /// Return the key for row @p id.
  std::string MakeKey(long id) const;  // NOLINT(google-runtime-int)

  /// Measure the time to compute an operation.
  template <typename Operation>
  static OperationResult TimeOperation(Operation&& op) {
    auto start = std::chrono::steady_clock::now();
    auto status = op();
    using std::chrono::duration_cast;
    auto elapsed = duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start);
    return OperationResult{status, elapsed};
  }

  /// Print the result of a throughput test in human readable form.
  static void PrintThroughputResult(std::ostream& os,
                                    std::string const& test_name,
                                    std::string const& phase,
                                    BenchmarkResult const& result);

  /// Print the result of a latency test in human readable form.
  static void PrintLatencyResult(std::ostream& os, std::string const& test_name,
                                 std::string const& operation,
                                 BenchmarkResult& result);

  /// Return the header for CSV results.
  static std::string ResultsCsvHeader();

  /// Print the result of a benchmark as a CSV line.
  void PrintResultCsv(std::ostream& os, std::string const& test_name,
                      std::string const& op_name,
                      std::string const& measurement,
                      BenchmarkResult& result) const;

  //@{
  /**
   * @name Embedded server counter accessors.
   *
   * Return 0 if there is no embedded server, or the value from the
   * corresponding embedded server counter.  This class is tested largely by
   * observing how many calls it makes on the embedded server.  Because the
   * embedded server has no memory, that is the only observable effect when
   * unit testing the class.
   */
  int create_table_count() const;
  int delete_table_count() const;
  int mutate_row_count() const;
  int mutate_rows_count() const;
  int read_rows_count() const;
  //@}

 private:
  /// Populate the table rows in the range [@p begin, @p end)
  google::cloud::StatusOr<BenchmarkResult> PopulateTableShard(
      bigtable::Table& table, long begin,  // NOLINT(google-runtime-int)
      long end);                           // NOLINT(google-runtime-int)

  /**
   * Return how much space to reserve for digits if the table has @p table_size
   * elements.
   */
  int KeyWidth() const;

 private:
  BenchmarkSetup setup_;
  int key_width_;
  bigtable::ClientOptions client_options_;
  std::unique_ptr<EmbeddedServer> server_;
  std::thread server_thread_;
};

/// Helper class to pretty print durations.
struct FormatDuration {
  template <typename Rep, typename Period>
  explicit FormatDuration(std::chrono::duration<Rep, Period> d)
      : ns(std::chrono::duration_cast<std::chrono::nanoseconds>(d)) {}
  std::chrono::nanoseconds ns;
};

/**
 * Pretty print an elapsed time.
 *
 * The benchmarks need to report time in human readable terms.  This operator
 * streams a FormatDuration in hours, minutes, seconds and sub-seconds.  Any
 * component that is zero gets ommitted, e.g. 1 hour exactly is printed as 1h.
 *
 * If the time is less than 1 second then the format uses millisecond or
 * microsecond resolution, as appropriate.
 *
 * @param os the destination stream.
 * @param duration the duration value.
 * @return the stream after printing.
 */
std::ostream& operator<<(std::ostream& os, FormatDuration duration);

}  // namespace benchmarks
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_BENCHMARK_H
