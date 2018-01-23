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

#include <chrono>
#include <future>
#include <iomanip>
#include <random>
#include <sstream>
#include "absl/strings/str_cat.h"
#include "bigtable/admin/admin_client.h"
#include "bigtable/admin/table_admin.h"
#include "bigtable/client/build_info.h"
#include "bigtable/client/table.h"

/**
 * @file
 *
 * Measure the latency of `bigtable::Table::Apply()` and
 * `bigtable::Table::ReadRow()`.
 *
 * This benchmark measures the latency of `bigtable::Table::Apply()` and
 * `bigtable::Table::ReadRow()` on a "typical" table serving data.  The
 * benchmark:
 * - Creates a table with 10,000,000 million rows, each row with a
 *   single column family, but with 10 columns, each column filled with a random
 *   100 byte string.
 * - The name of the table starts with `perf`, followed by
 *   `kTableIdRandomLetters` selected at random.
 * - Report the running time and throughout in this phase.
 * - The benchmark then runs for S seconds, constantly executing this basic
 *   block in T parallel threads:
 *   - Randomly, with 50% probability, pick if the next operation is an
 *     `Apply()` or a `ReadRow()`.
 *   - If the operation is a `ReadRow()` pick one of the 10,000,000 keys at
 *     random, with uniform probability, then perform the operation, record the
 *     latency and whether the operation was successful.
 *   - If the operation is an `Apply()`, pick new values for all the fields at
 *     random, then perform the operation, record the latency and whether the
 *     operation was successful.
 * - Collect the results from all the threads.
 * - Report the results, including p0 (minimum), p50, p90, p95, p99, p99.9, and
 *   p100 (maximum) latencies.
 * - Report the number of operations of each type, the total running time, and
 *   the effective throughput.
 * - Delete the table.
 *
 * The test can be configured to create a local gRPC server that implements the
 * Cloud Bigtable API (`google.bigtable.v2.Bigtable` to be precise) via the
 * command-line.  Otherwise, the default configuration is used, that is,
 * a production instance of Cloud Bigtable unless the CLOUD_BIGTABLE_EMULATOR
 * environment variable is set.
 */

/// Helper functions and types for the apply_read_latency_benchmark.
namespace {
struct OperationResult {
  bool successful;
  std::chrono::microseconds latency;
};

struct BenchmarkResult {
  std::vector<OperationResult> apply_results;
  std::vector<OperationResult> read_results;
};

class Server {
 public:
  virtual ~Server() = default;

  virtual void Shutdown() = 0;
  virtual void Wait() = 0;
};

/// Create an embedded server.
std::unique_ptr<Server> CreateEmbeddedServer(int port);

/// Obtain the test annotations: (start_time, version/compiler-info)
std::pair<std::string, std::string> TestAnnotations();

/// Populate the table with initial data.
void PopulateTable(std::shared_ptr<bigtable::DataClient> data_client,
                   std::string const& table_id, long table_size,
                   std::pair<std::string, std::string> const& annotations);

/// Run an iteration of the test.
BenchmarkResult RunBenchmark(std::shared_ptr<bigtable::DataClient> data_client,
                             std::string const& table_id, long table_size,
                             std::chrono::seconds test_duration);

/// Print the results of one operation.
void PrintResults(std::string const& operation,
                  std::vector<OperationResult>& results,
                  std::pair<std::string, std::string> const& annotations,
                  std::chrono::milliseconds elapsed);

/// Sample from a string.
std::string SampleWithRepetition(std::mt19937_64& generator,
                                 std::size_t sample_size,
                                 std::string const& values);

/// Initialize a pseudo-random number generator.
std::mt19937_64 InitializePRNG();

//@{
/// @name Test constants.  Defined as requirements in the original bug (#189).

/// The size of the table for this test.
constexpr long kNumRowKeys = 10 * 1000 * 1000;  // C++17 separators I miss you.

/**
 * The width of the numeric suffix for each row key.
 * The row keys are formed by appending a fixed number of digits to `user`.
 * This controls the number of digits, and should match the width required for
 * `kNumRowKeys`.  It is hard-coded because the puzzle of figuring out the
 * width is fun programming, but not that useful.  Use `constexpr` functions if
 * you want a really cool version of the puzzle.
 */
constexpr int kKeyWidth = 7;

/// The name of the col
constexpr char kColumnFamily[] = "cf";

/// The number of fields (aka columns, aka column qualifiers) in each row.
constexpr int kNumFields = 10;

/// The size of each value.
constexpr int kFieldSize = 100;

/// The size of each BulkApply request.
constexpr long kBulkSize = 1000;

/// The number of threads running the latency test.
constexpr int kDefaultThreads = 4;

/// How long does the test last by default.
constexpr int kDefaultTestDuration = 30;

/// How many shards are used to populate the table.
constexpr int kPopulateShardCount = 10;

/// How many times each thread to populate the table reports progress.
constexpr int kPopulateShardProgressMarks = 4;

/// How many random bytes in the table id.
constexpr int kTableIdRandomLetters = 8;
//@}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  if (argc < 3) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project> <instance>"
              << " [thread-count (" << kDefaultThreads << ")]"
              << " [test-duration-seconds (" << kDefaultTestDuration << "min)]"
              << " [table-size (" << kNumRowKeys << ")]"
              << " [embedded-server-port (use default config if not set)]"
              << std::endl;
    return 1;
  }
  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];

  auto generator = InitializePRNG();
  static std::string const table_id_chars(
      "ABCDEFGHIJLKMNOPQRSTUVWXYZabcdefghijlkmnopqrstuvwxyz0123456789-_");

  std::string const table_id =
      "perf" +
      SampleWithRepetition(generator, kTableIdRandomLetters, table_id_chars);

  int thread_count = kDefaultThreads;
  if (argc > 3) {
    thread_count = std::stoi(argv[3]);
  }

  std::chrono::seconds test_duration(kDefaultTestDuration * 60);
  if (argc > 4) {
    test_duration = std::chrono::seconds(std::stol(argv[4]));
  }

  long table_size = kNumRowKeys;
  if (argc > 5) {
    table_size = std::stoi(argv[5]);
  }

  int port = 0;
  if (argc > 6) {
    port = std::stoi(argv[6]);
  }

  bigtable::ClientOptions client_options;
  std::unique_ptr<Server> server;
  std::thread server_thread;

  if (port != 0) {
    server = CreateEmbeddedServer(port);
    server_thread = std::thread([&server]() { server->Wait(); });

    client_options.set_admin_endpoint("localhost:" + std::to_string(port));
    client_options.set_data_endpoint("localhost:" + std::to_string(port));
    client_options.SetCredentials(grpc::InsecureChannelCredentials());
  }

  auto annotations = TestAnnotations();
  std::cout << "Name,start,nsamples,min,p50,p90,p95,p99,p99.9,max,throughput"
            << ",notes" << std::endl;

  // Create the table, with an initial split.
  bigtable::TableAdmin admin(
      bigtable::CreateDefaultAdminClient(project_id, client_options),
      instance_id);
  std::vector<std::string> splits{"user0", "user1", "user2", "user3", "user4",
                                  "user5", "user6", "user7", "user8", "user9"};
  auto schema = admin.CreateTable(
      table_id,
      bigtable::TableConfig(
          {{kColumnFamily, bigtable::GcRule::MaxNumVersions(1)}}, splits));

  auto data_client = bigtable::CreateDefaultDataClient(project_id, instance_id,
                                                       client_options);

  // Populate the table
  PopulateTable(data_client, table_id, table_size, annotations);

  // Start the threads running the latency test.
  auto latency_test_start = std::chrono::steady_clock::now();
  std::vector<std::future<BenchmarkResult>> tasks;
  for (int i = 0; i != thread_count; ++i) {
    auto launch_policy = std::launch::async;
    if (thread_count == 1) {
      // If the user requests only one thread, use the current thread.
      launch_policy = std::launch::deferred;
    }
    tasks.emplace_back(std::async(launch_policy, RunBenchmark, data_client,
                                  table_id, table_size, test_duration));
  }

  // Wait for the threads and combine all the results.
  BenchmarkResult combined;
  int count = 0;
  auto append = [](std::vector<OperationResult>& destination,
                   std::vector<OperationResult> const& source) {
    destination.insert(destination.end(), source.begin(), source.end());
  };
  for (auto& future : tasks) {
    try {
      auto result = future.get();
      append(combined.apply_results, result.apply_results);
      append(combined.read_results, result.read_results);
    } catch (std::exception const& ex) {
      std::cerr << "Standard exception raised by task[" << count
                << "]: " << ex.what() << std::endl;
    }
    ++count;
  }
  auto latency_test_elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - latency_test_start);

  PrintResults("Apply()", combined.apply_results, annotations,
               latency_test_elapsed);
  PrintResults("ReadRow()", combined.read_results, annotations,
               latency_test_elapsed);

  std::vector<OperationResult> overall = combined.apply_results;
  append(overall, combined.read_results);
  PrintResults("all-ops", overall, annotations, latency_test_elapsed);

  admin.DeleteTable(table_id);

  if (server) {
    server->Shutdown();
    server_thread.join();
  }

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << std::endl;
  return 1;
}

namespace {
std::pair<std::string, std::string> TestAnnotations() {
  auto start = std::chrono::system_clock::now();
  std::time_t start_c = std::chrono::system_clock::to_time_t(start);
  std::string formatted("YYYY-MM-DDTHH:SS:MMZ");
  std::strftime(&formatted[0], formatted.size(), "%FT%TZ",
                std::gmtime(&start_c));

  std::string notes =
      absl::StrCat(bigtable::version_string(), ";", bigtable::compiler, ";",
                   bigtable::compiler_flags);
  for (auto& c : notes) {
    if (c == '\n') {
      c = ';';
    }
  }

  return std::make_pair(formatted, notes);
}

std::string SampleWithRepetition(std::mt19937_64& generator,
                                 std::size_t sample_size,
                                 std::string const& values) {
  std::uniform_int_distribution<std::size_t> rd(0, values.size());

  std::string result(sample_size, '0');
  std::generate(result.begin(), result.end(),
                [&rd, &generator, &values]() { return values[rd(generator)]; });
  return result;
}

std::string MakeRandomValue(std::mt19937_64& generator) {
  static std::string const letters(
      "ABCDEFGHIJLKMNOPQRSTUVWXYZabcdefghijlkmnopqrstuvwxyz0123456789-/_");
  return SampleWithRepetition(generator, kFieldSize, letters);
}

bigtable::Mutation MakeRandomMutation(std::mt19937_64& generator, int fieldno) {
  std::string field = "field" + std::to_string(fieldno);
  return bigtable::SetCell(kColumnFamily, std::move(field), 0,
                           MakeRandomValue(generator));
}

OperationResult RunOneApply(bigtable::Table& table, std::string row_key,
                            std::mt19937_64& generator) {
  bigtable::SingleRowMutation mutation(std::move(row_key));
  for (int field = 0; field != kNumFields; ++field) {
    mutation.emplace_back(MakeRandomMutation(generator, field));
  }
  using namespace std::chrono;
  auto start = std::chrono::steady_clock::now();
  bool successful = false;
  try {
    table.Apply(std::move(mutation));
    successful = true;
  } catch (...) {
  }
  auto elapsed =
      duration_cast<microseconds>(std::chrono::steady_clock::now() - start);
  return OperationResult{successful, elapsed};
}

OperationResult RunOneReadRow(bigtable::Table& table, std::string row_key) {
  using namespace std::chrono;
  auto start = std::chrono::steady_clock::now();
  bool successful = false;
  try {
    auto row = table.ReadRow(
        std::move(row_key),
        bigtable::Filter::ColumnRangeClosed(kColumnFamily, "field0", "field9"));
    successful = true;
  } catch (...) {
  }
  auto elapsed =
      duration_cast<microseconds>(std::chrono::steady_clock::now() - start);
  return OperationResult{successful, elapsed};
}

std::mt19937_64 InitializePRNG() {
  // While std::mt19937_64 is not the best PRNG ever, it is fairly good for
  // most purposes.  Please read:
  //    http://www.pcg-random.org/
  // for a discussion on the topic of PRNG in general, and the C++ generators in
  // particular.

  // TODO() - this code is generic, turn in into a template function.
  using Generator = std::mt19937_64;

  // We use the default C++ random device to generate entropy.  The quality of
  // this source of entropy is implementation-defined, on Linux (libstdc++ with
  // modern versions of the compiler) it is based on either `/dev/urandom`, or
  // (where available) the RDRND CPU instruction.
  //     http://en.cppreference.com/w/cpp/numeric/random/random_device/random_device
  std::random_device rd;

  // We need to get enough entropy to fully initialize the PRNG, that depends on
  // the size of its state.  The size in bits is Generator::state_size, so we
  // convert that to the number of `unsigned int` values; as this is what
  // std::random_device returns:
  auto const S =
      Generator::state_size *
      (Generator::word_size / std::numeric_limits<unsigned int>::digits);
  std::vector<unsigned int> entropy(S);
  std::generate(entropy.begin(), entropy.end(), [&rd]() { return rd(); });

  // This last bit is just an annoying fact about how C++11 wants to represent
  // a seed to initialize a PRNG.  See this for a critique of the C++11
  // approach:
  //   http://www.pcg-random.org/posts/simple-portable-cpp-seed-entropy.html
  std::seed_seq seq(entropy.begin(), entropy.end());
  return Generator(seq);
}

BenchmarkResult RunBenchmark(std::shared_ptr<bigtable::DataClient> data_client,
                             std::string const& table_id, long table_size,
                             std::chrono::seconds test_duration) {
  BenchmarkResult result = {};

  bigtable::Table table(std::move(data_client), table_id);

  auto generator = InitializePRNG();
  std::uniform_int_distribution<long> prng_user(0, table_size);
  std::uniform_int_distribution<int> prng_operation(0, 1);

  auto test_start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() < test_start + test_duration) {
    std::ostringstream os;
    os << "user" << std::setw(kKeyWidth) << std::setfill('0')
       << prng_user(generator);

    if (prng_operation(generator) == 0) {
      result.apply_results.emplace_back(
          RunOneApply(table, os.str(), generator));
    } else {
      result.read_results.emplace_back(RunOneReadRow(table, os.str()));
    }
  }
  return result;
}

void PopulateTableShard(bigtable::Table& table, long begin, long end) {
  auto generator = InitializePRNG();
  int bulk_size = 0;
  bigtable::BulkMutation bulk;

  long progress_period = (end - begin) / kPopulateShardProgressMarks;
  if (progress_period == 0) {
    progress_period = (end - begin);
  }
  for (long idx = begin; idx != end; ++idx) {
    std::ostringstream os;
    os << "user" << std::setw(kKeyWidth) << std::setfill('0') << idx;
    bigtable::SingleRowMutation mutation(os.str());
    std::vector<bigtable::Mutation> columns;
    for (int f = 0; f != kNumFields; ++f) {
      mutation.emplace_back(MakeRandomMutation(generator, f));
    }
    bulk.emplace_back(std::move(mutation));
    if (++bulk_size >= kBulkSize) {
      table.BulkApply(std::move(bulk));
      bulk = {};
      bulk_size = 0;
    }
    long count = idx - begin + 1;
    if (count % progress_period == 0) {
      std::cout << "." << std::flush;
    }
  }
  if (bulk_size != 0) {
    table.BulkApply(std::move(bulk));
  }
}

void PopulateTable(std::shared_ptr<bigtable::DataClient> data_client,
                   std::string const& table_id, long table_size,
                   std::pair<std::string, std::string> const& annotations) {
  bigtable::Table table(std::move(data_client), table_id);

  std::vector<std::future<void>> tasks;
  long start = 0;
  auto upload_start = std::chrono::steady_clock::now();
  std::cout << "# Populating table " << std::flush;
  for (int i = 0; i != kPopulateShardCount; ++i) {
    std::string prefix = "user" + std::to_string(i);
    long end = std::min(table_size, start + table_size / kPopulateShardCount);
    tasks.emplace_back(std::async(std::launch::async, [&table, start, end]() {
      PopulateTableShard(table, start, end);
    }));
    start = end;
  }
  int count = 0;
  for (auto& t : tasks) {
    try {
      t.get();
    } catch (std::exception const& ex) {
      std::cerr << "Exception raised by PopulateTask/" << count << ": "
                << ex.what() << std::endl;
    }
    ++count;
  }
  std::cout << " DONE" << std::endl;
  auto upload_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - upload_start);
  auto upload_throughput = 1000 * table_size / upload_elapsed.count();
  std::cout << "# Bulk load throughput=" << upload_throughput << " finished in "
            << upload_elapsed.count() << "ms" << std::endl;
  std::cout << "Upload," << std::get<0>(annotations) << "," << table_size
            << ",,,,,,,," << upload_throughput << ","
            << std::get<1>(annotations) << std::endl;
}

void PrintResults(std::string const& operation, std::string const& qualifier,
                  std::vector<OperationResult>::iterator begin,
                  std::vector<OperationResult>::iterator end,
                  std::pair<std::string, std::string> const& annotations,
                  std::chrono::milliseconds elapsed) {
  long nsamples = std::distance(begin, end);
  if (nsamples == 0) {
    return;
  }
  // sort the results by latency.
  std::sort(begin, end,
            [](OperationResult const& lhs, OperationResult const& rhs) {
              return lhs.latency < rhs.latency;
            });

  // Prepare a CSV line for computer-parseable output.
  std::ostringstream msg;
  std::cout << operation << qualifier << "," << std::get<0>(annotations) << ","
            << nsamples;
  auto throughput = 1000 * nsamples / elapsed.count();
  msg << "# " << operation << qualifier << " Throughput = " << throughput
      << " ops/s, Latency: ";
  double percentiles[] = {0, 50, 90, 95, 99, 99.9, 100};
  char const* sep = "";
  for (double p : percentiles) {
    auto index =
        static_cast<std::size_t>(std::round((nsamples - 1) * p / 100.0));
    auto i = begin;
    std::advance(i, index);
    std::cout << "," << i->latency.count();
    msg << sep << "p" << std::setprecision(3) << p << "="
        << std::setprecision(2) << i->latency.count() / 1000.0;
    sep = "ms, ";
  }
  std::cout << "," << throughput << "," << std::get<1>(annotations)
            << std::endl;
  std::cout << msg.str() << std::endl;
}

void PrintResults(std::string const& operation,
                  std::vector<OperationResult>& results,
                  std::pair<std::string, std::string> const& annotations,
                  std::chrono::milliseconds elapsed) {
  // First split the results into those with errors and those without errors.
  auto partition_point =
      std::partition(results.begin(), results.end(),
                     [](OperationResult const& a) { return a.successful; });
  PrintResults(operation, "/Success", results.begin(), partition_point,
               annotations, elapsed);
  PrintResults(operation, "/Failure", partition_point, results.end(),
               annotations, elapsed);
}

namespace btproto = google::bigtable::v2;
namespace adminproto = google::bigtable::admin::v2;

class BigtableImpl final : public btproto::Bigtable::Service {
 public:
  grpc::Status MutateRow(grpc::ServerContext* context,
                         btproto::MutateRowRequest const* request,
                         btproto::MutateRowResponse* response) override {
    return grpc::Status::OK;
  }

  grpc::Status MutateRows(
      grpc::ServerContext* context, btproto::MutateRowsRequest const* request,
      grpc::ServerWriter<btproto::MutateRowsResponse>* writer) override {
    btproto::MutateRowsResponse msg;
    for (int index = 0; index != request->entries_size(); ++index) {
      auto& entry = *msg.add_entries();
      entry.set_index(index);
      entry.mutable_status()->set_code(grpc::StatusCode::OK);
    }
    writer->WriteLast(msg, grpc::WriteOptions());
    return grpc::Status::OK;
  }

  grpc::Status ReadRows(
      grpc::ServerContext* context,
      google::bigtable::v2::ReadRowsRequest const* request,
      grpc::ServerWriter<google::bigtable::v2::ReadRowsResponse>* writer)
      override {
    if (request->rows_limit() != 1 or request->rows().row_keys_size() != 1) {
      return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not-yet");
    }
    auto const& row_key = request->rows().row_keys(0);
    btproto::ReadRowsResponse msg;
    msg.set_last_scanned_row_key(row_key);
    auto& chunk = *msg.add_chunks();
    chunk.set_row_key(row_key);
    chunk.set_timestamp_micros(0);
    chunk.mutable_family_name()->set_value(kColumnFamily);
    chunk.mutable_qualifier()->set_value("field0");
    chunk.set_value("01234566789");
    chunk.set_commit_row(true);
    writer->WriteLast(msg, grpc::WriteOptions());
    return grpc::Status::OK;
  }
};

class TableAdminImpl final : public adminproto::BigtableTableAdmin::Service {
 public:
  grpc::Status CreateTable(
      grpc::ServerContext* context,
      google::bigtable::admin::v2::CreateTableRequest const* request,
      google::bigtable::admin::v2::Table* response) override {
    response->set_name(request->parent() + "/tables/" + request->table_id());
    return grpc::Status::OK;
  }

  grpc::Status DeleteTable(
      grpc::ServerContext* context,
      google::bigtable::admin::v2::DeleteTableRequest const* request,
      ::google::protobuf::Empty* response) override {
    return grpc::Status::OK;
  }
};

class EmbeddedServer : public Server {
 public:
  explicit EmbeddedServer(int port) {
    std::string server_address("0.0.0.0:" + std::to_string(port));
    builder_.AddListeningPort(server_address,
                              grpc::InsecureServerCredentials());
    builder_.RegisterService(&bigtable_service_);
    builder_.RegisterService(&admin_service_);
    server_ = builder_.BuildAndStart();
  }

  void Shutdown() override { server_->Shutdown(); }
  void Wait() override { server_->Wait(); }

 private:
  BigtableImpl bigtable_service_;
  TableAdminImpl admin_service_;
  grpc::ServerBuilder builder_;
  std::unique_ptr<grpc::Server> server_;
};

std::unique_ptr<Server> CreateEmbeddedServer(int port) {
  return std::unique_ptr<Server>(new EmbeddedServer(port));
}
}  // anonymous namespace
