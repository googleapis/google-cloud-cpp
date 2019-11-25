// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/internal/build_info.h"
#include "google/cloud/spanner/internal/compiler_info.h"
#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/grpc_utils/grpc_error_delegate.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include <algorithm>
#include <future>
#include <random>
#include <sstream>
#include <thread>
#if GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
#include <google/spanner/v1/result_set.pb.h>
#include <sys/resource.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE

namespace {

using google::cloud::Status;
namespace cs = google::cloud::spanner;

/**
 * @file
 *
 * A CPU cost per call benchmark for the Cloud Spanner C++ client library.
 *
 * This program measures the CPU cost of multiple single-row operations in the
 * client library. Other techniques, such as using the time(1) program, can
 * yield inaccurate results as the setup costs (creating a table, populating it
 * with some initial data) can be very high.
 */

struct Config {
  std::string experiment;

  std::string project_id;
  std::string instance_id;
  std::string database_id;

  int samples = 2;
  std::chrono::seconds iteration_duration = std::chrono::seconds(5);

  int minimum_threads = 1;
  int maximum_threads = 1;
  int minimum_clients = 1;
  int maximum_clients = 1;

  std::int64_t table_size = 1000 * 1000L;
  std::int64_t query_size = 1000;

  bool use_only_clients = false;
  bool use_only_stubs = false;
};

std::ostream& operator<<(std::ostream& os, Config const& config);

struct RowCpuSample {
  int client_count;
  int thread_count;
  bool using_stub;
  int row_count;
  std::chrono::microseconds elapsed;
  std::chrono::microseconds cpu_time;
  Status status;
};

std::ostream& operator<<(std::ostream& os, RowCpuSample const& s) {
  return os << std::boolalpha << s.client_count << ',' << s.thread_count << ','
            << s.using_stub << ',' << s.row_count << ',' << s.elapsed.count()
            << ',' << s.cpu_time.count() << ',' << s.status.code();
}

class Experiment {
 public:
  virtual ~Experiment() = default;

  virtual Status SetUp(Config const& config, cs::Database const& database) = 0;
  virtual Status TearDown(Config const& config,
                          cs::Database const& database) = 0;
  virtual Status Run(Config const& config, cs::Database const& database) = 0;
};

using ExperimentFactory = std::function<std::unique_ptr<Experiment>(
    google::cloud::internal::DefaultPRNG)>;

std::map<std::string, ExperimentFactory> AvailableExperiments();

google::cloud::StatusOr<Config> ParseArgs(std::vector<std::string> args);

}  // namespace

int main(int argc, char* argv[]) {
  Config config;
  {
    std::vector<std::string> args{argv, argv + argc};
    auto c = ParseArgs(args);
    if (!c) {
      std::cerr << "Error parsing command-line arguments: " << c.status()
                << "\n";
      return 1;
    }
    config = *std::move(c);
  }

  auto available = AvailableExperiments();
  auto e = available.find(config.experiment);
  if (e == available.end()) {
    std::cerr << "Experiment " << config.experiment << " not found\n";
    return 1;
  }

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  if (config.instance_id.empty()) {
    auto instance = google::cloud::spanner_testing::PickRandomInstance(
        generator, config.project_id);
    if (!instance) {
      std::cerr << "Error selecting an instance to run the experiment: "
                << instance.status() << "\n";
      return 1;
    }
    config.instance_id = *std::move(instance);
  }

  cs::Database database(
      config.project_id, config.instance_id,
      google::cloud::spanner_testing::RandomDatabaseName(generator));
  config.database_id = database.database_id();

  // Once the configuration is fully initialized and the database name set,
  // print everything out.
  std::cout << config << std::flush;

  cs::DatabaseAdminClient admin_client;
  auto created = admin_client.CreateDatabase(database, {});
  std::cout << "# Waiting for database creation to complete " << std::flush;
  for (;;) {
    auto status = created.wait_for(std::chrono::seconds(1));
    if (status == std::future_status::ready) break;
    std::cout << '.' << std::flush;
  }
  std::cout << " DONE\n";
  auto db = created.get();
  if (!db) {
    std::cerr << "Error creating database: " << db.status() << "\n";
    return 1;
  }

  std::cout << "ClientCount,ThreadCount,UsingStub"
            << ",RowCount,ElapsedTime,CpuTime,StatusCode\n"
            << std::flush;

  int exit_status = EXIT_SUCCESS;

  auto experiment = e->second(generator);
  auto status = experiment->SetUp(config, database);
  if (!status.ok()) {
    std::cout << "# Skipping experiment, SetUp() failed: " << status << "\n";
    exit_status = EXIT_FAILURE;
  } else {
    status = experiment->Run(config, database);
    if (!status.ok()) exit_status = EXIT_FAILURE;
    (void)experiment->TearDown(config, database);
  }

  auto drop = admin_client.DropDatabase(database);
  if (!drop.ok()) {
    std::cerr << "# Error dropping database: " << drop << "\n";
  }
  std::cout << "# Experiment finished, database dropped\n";
  return exit_status;
}

namespace {

std::ostream& operator<<(std::ostream& os, Config const& config) {
  return os << std::boolalpha << "# Experiment: " << config.experiment
            << "\n# Project: " << config.project_id
            << "\n# Instance: " << config.instance_id
            << "\n# Database: " << config.database_id
            << "\n# Samples: " << config.samples
            << "\n# Minimum Threads: " << config.minimum_threads
            << "\n# Maximum Threads: " << config.maximum_threads
            << "\n# Minimum Clients: " << config.minimum_clients
            << "\n# Maximum Clients: " << config.maximum_clients
            << "\n# Iteration Duration: " << config.iteration_duration.count()
            << "s"
            << "\n# Table Size: " << config.table_size
            << "\n# Query Size: " << config.query_size
            << "\n# Use Only Stubs: " << config.use_only_stubs
            << "\n# Use Only Clients: " << config.use_only_clients
            << "\n# Compiler: " << cs::internal::CompilerId() << "-"
            << cs::internal::CompilerVersion()
            << "\n# Build Flags: " << cs::internal::BuildFlags() << "\n";
}

bool SupportPerThreadUsage();

class SimpleTimer {
 public:
  SimpleTimer() : elapsed_time_(0), cpu_time_(0) {}

  /// Start the timer, call before the code being measured.
  void Start();

  /// Stop the timer, call after the code being measured.
  void Stop();

  //@{
  /**
   * @name Measurement results.
   *
   * @note The values are only valid after calling Start() and Stop().
   */
  // NOLINTNEXTLINE(readability-identifier-naming)
  std::chrono::microseconds elapsed_time() const { return elapsed_time_; }
  // NOLINTNEXTLINE(readability-identifier-naming)
  std::chrono::microseconds cpu_time() const { return cpu_time_; }
  // NOLINTNEXTLINE(readability-identifier-naming)
  std::string const& annotations() const { return annotations_; }
  //@}

 private:
  std::chrono::steady_clock::time_point start_;
  std::chrono::microseconds elapsed_time_;
  std::chrono::microseconds cpu_time_;
  std::string annotations_;
#if GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
  struct rusage start_usage_ = {};
#endif  // GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
};

struct BoolTraits {
  using native_type = bool;
  static std::string SpannerDataType() { return "BOOL"; }
  static std::string TableSuffix() { return "bool"; }
  static native_type MakeRandomValue(
      google::cloud::internal::DefaultPRNG& generator) {
    return std::uniform_int_distribution<int>(0, 1)(generator) == 1;
  }
};

struct BytesTraits {
  using native_type = cs::Bytes;
  static std::string SpannerDataType() { return "BYTES(1024)"; }
  static std::string TableSuffix() { return "bytes"; }
  static native_type MakeRandomValue(
      google::cloud::internal::DefaultPRNG& generator) {
    static std::string const kPopulation = [] {
      std::string result;
      for (int c = std::numeric_limits<char>::min();
           c <= std::numeric_limits<char>::max(); ++c) {
        result.push_back(static_cast<char>(c));
      }
      return result;
    }();
    std::string tmp =
        google::cloud::internal::Sample(generator, 1024, kPopulation);
    return cs::Bytes(tmp.begin(), tmp.end());
  }
};

struct DateTraits {
  using native_type = cs::Date;
  static std::string SpannerDataType() { return "DATE"; }
  static std::string TableSuffix() { return "date"; }
  static native_type MakeRandomValue(
      google::cloud::internal::DefaultPRNG& generator) {
    return {std::uniform_int_distribution<std::int64_t>(1, 2000)(generator),
            std::uniform_int_distribution<int>(1, 12)(generator),
            std::uniform_int_distribution<int>(1, 28)(generator)};
  }
};

struct Float64Traits {
  using native_type = double;
  static std::string SpannerDataType() { return "FLOAT64"; }
  static std::string TableSuffix() { return "float64"; }
  static native_type MakeRandomValue(
      google::cloud::internal::DefaultPRNG& generator) {
    return std::uniform_real_distribution<double>(0.0, 1.0)(generator);
  }
};

struct Int64Traits {
  using native_type = std::int64_t;
  static std::string SpannerDataType() { return "INT64"; }
  static std::string TableSuffix() { return "int64"; }
  static native_type MakeRandomValue(
      google::cloud::internal::DefaultPRNG& generator) {
    return std::uniform_int_distribution<std::int64_t>(
        std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::max())(generator);
  }
};

struct StringTraits {
  using native_type = std::string;
  static std::string SpannerDataType() { return "STRING(1024)"; }
  static std::string TableSuffix() { return "string"; }
  static native_type MakeRandomValue(
      google::cloud::internal::DefaultPRNG& generator) {
    return google::cloud::internal::Sample(
        generator, 1024, "#@$%^&*()-=+_0123456789[]{}|;:,./<>?");
  }
};

struct TimestampTraits {
  using native_type = cs::Timestamp;
  static std::string SpannerDataType() { return "TIMESTAMP"; }
  static std::string TableSuffix() { return "timestamp"; }
  static native_type MakeRandomValue(
      google::cloud::internal::DefaultPRNG& generator) {
    using rep = cs::Timestamp::duration::rep;
    return cs::Timestamp(
        cs::Timestamp::duration(std::uniform_int_distribution<rep>(
            0, std::numeric_limits<rep>::max())(generator)));
  }
};

template <typename Traits>
class ExperimentImpl {
 public:
  explicit ExperimentImpl(google::cloud::internal::DefaultPRNG const& generator)
      : generator_(generator) {}

  Status CreateTable(Config const&, cs::Database const& database,
                     std::string const& table_name) {
    std::string statement = "CREATE TABLE " + table_name;
    statement += " (Key INT64 NOT NULL,\n";
    for (int i = 0; i != 10; ++i) {
      statement +=
          "Data" + std::to_string(i) + " " + Traits::SpannerDataType() + ",\n";
    }
    statement += ") PRIMARY KEY (Key)";
    cs::DatabaseAdminClient admin_client;
    auto created = admin_client.UpdateDatabase(database, {statement});
    std::cout << "# Waiting for table creation to complete " << std::flush;
    for (;;) {
      auto status = created.wait_for(std::chrono::seconds(1));
      if (status == std::future_status::ready) break;
      std::cout << '.' << std::flush;
    }
    std::cout << " DONE\n";
    auto db = created.get();
    if (!db) {
      std::cerr << "Error creating table: " << db.status() << "\n";
      return std::move(db).status();
    }
    return {};
  }

  Status FillTable(Config const& config, cs::Database const& database,
                   std::string const& table_name) {
    auto status = CreateTable(config, database, table_name);
    if (!status.ok()) return status;

    // We need to populate some data or all the requests to read will fail.
    cs::Client client(cs::MakeConnection(database));
    std::cout << "# Populating database " << std::flush;
    int const task_count = 16;
    std::vector<std::future<void>> tasks(task_count);
    int task_id = 0;
    for (auto& t : tasks) {
      t = std::async(
          std::launch::async,
          [this, &config, &client, &table_name](int tc, int ti) {
            FillTableTask(config, client, table_name, tc, ti);
          },
          task_count, task_id++);
    }
    for (auto& t : tasks) {
      t.get();
    }
    std::cout << " DONE\n";
    return {};
  }

  typename Traits::native_type GenerateRandomValue() {
    std::lock_guard<std::mutex> lk(mu_);
    return Traits::MakeRandomValue(generator_);
  }

  std::int64_t RandomKey(Config const& config) {
    std::lock_guard<std::mutex> lk(mu_);
    return std::uniform_int_distribution<std::int64_t>(
        0, config.table_size - 1)(generator_);
  }

  cs::KeySet RandomKeySet(Config const& config) {
    std::lock_guard<std::mutex> lk(mu_);
    auto begin = std::uniform_int_distribution<std::int64_t>(
        0, config.table_size - config.query_size)(generator_);
    auto end = begin + config.query_size - 1;
    return cs::KeySet().AddRange(cs::MakeKeyBoundClosed(cs::Value(begin)),
                                 cs::MakeKeyBoundClosed(cs::Value(end)));
  }

  bool UseStub(Config const& config) {
    if (config.use_only_clients) {
      return false;
    }
    if (config.use_only_stubs) {
      return true;
    }
    std::lock_guard<std::mutex> lk(mu_);
    return std::uniform_int_distribution<int>(0, 1)(generator_) == 1;
  }

  int ThreadCount(Config const& config) {
    std::lock_guard<std::mutex> lk(mu_);
    return std::uniform_int_distribution<int>(
        config.minimum_threads, config.maximum_threads)(generator_);
  }

  int ClientCount(Config const& config, int thread_count) {
    // TODO(#1000) - avoid deadlocks with more than 100 threads per client
    auto const min_clients =
        (std::max<int>)(thread_count / 100 + 1, config.minimum_clients);
    auto const max_clients = config.maximum_clients;
    if (min_clients <= max_clients) {
      return min_clients;
    }
    std::lock_guard<std::mutex> lk(mu_);
    return std::uniform_int_distribution<int>(min_clients,
                                              max_clients - 1)(generator_);
  }

  /// Get a snapshot of the random bit generator
  google::cloud::internal::DefaultPRNG Generator() const {
    std::lock_guard<std::mutex> lk(mu_);
    return generator_;
  };

  void DumpSamples(std::vector<RowCpuSample> const& samples) const {
    std::lock_guard<std::mutex> lk(mu_);
    std::copy(samples.begin(), samples.end(),
              std::ostream_iterator<RowCpuSample>(std::cout, "\n"));
    auto it =
        std::find_if(samples.begin(), samples.end(),
                     [](RowCpuSample const& x) { return !x.status.ok(); });
    if (it == samples.end()) {
      return;
    }
    std::cout << "# FIRST ERROR: " << it->status << "\n";
  }

  void LogError(std::string const& s) {
    std::lock_guard<std::mutex> lk(mu_);
    std::cout << "# " << s << std::endl;
  }

 private:
  Status FillTableTask(Config const& config, cs::Client client,
                       std::string const& table_name, int task_count,
                       int task_id) {
    std::vector<std::string> const column_names{
        "Key",   "Data0", "Data1", "Data2", "Data3", "Data4",
        "Data5", "Data6", "Data7", "Data8", "Data9"};
    using T = typename Traits::native_type;
    T value0 = GenerateRandomValue();
    T value1 = GenerateRandomValue();
    T value2 = GenerateRandomValue();
    T value3 = GenerateRandomValue();
    T value4 = GenerateRandomValue();
    T value5 = GenerateRandomValue();
    T value6 = GenerateRandomValue();
    T value7 = GenerateRandomValue();
    T value8 = GenerateRandomValue();
    T value9 = GenerateRandomValue();

    auto mutation = cs::InsertOrUpdateMutationBuilder(table_name, column_names);
    int current_mutations = 0;

    auto maybe_flush = [&, this](bool force) -> Status {
      if (current_mutations == 0) {
        return {};
      }
      if (!force && current_mutations < 1000) {
        return {};
      }
      auto m = std::move(mutation).Build();
      auto result = client.Commit(
          [&m](cs::Transaction const&) { return cs::Mutations{m}; });
      if (!result) {
        std::lock_guard<std::mutex> lk(mu_);
        std::cerr << "# Error in Commit() " << result.status() << "\n";
        return std::move(result).status();
      }
      mutation = cs::InsertOrUpdateMutationBuilder(table_name, column_names);
      current_mutations = 0;
      return {};
    };
    auto force_flush = [&maybe_flush] { return maybe_flush(true); };
    auto flush_as_needed = [&maybe_flush] { return maybe_flush(false); };

    auto const report_period =
        (std::max)(static_cast<std::int64_t>(2), config.table_size / 50);
    for (std::int64_t key = 0; key != config.table_size; ++key) {
      // Each thread does a fraction of the key space.
      if (key % task_count != task_id) continue;
      // Have one of the threads report progress about 50 times.
      if (task_id == 0 && key % report_period == 0) {
        std::cout << '.' << std::flush;
      }
      mutation.EmplaceRow(key, value0, value1, value2, value3, value4, value5,
                          value6, value7, value8, value9);
      ++current_mutations;
      auto status = flush_as_needed();
      if (!status.ok()) return status;
    }
    return force_flush();
  }

  mutable std::mutex mu_;
  google::cloud::internal::DefaultPRNG generator_;
};

/**
 * Run an experiment to measure the CPU overhead of the client over raw gRPC.
 *
 * This experiments creates and populates a table with K rows, each row
 * containing an (integer) key and 10 columns of the types defined by `Traits`.
 * Then the experiment performs M iterations of:
 *   - Randomly select if it will do the work using the client library or raw
 *     gRPC
 *   - Then for N seconds read random rows
 *   - Measure the CPU time required by the previous step
 *
 * The values of K, M, N are configurable.
 */
template <typename Traits>
class ReadExperiment : public Experiment {
 public:
  explicit ReadExperiment(google::cloud::internal::DefaultPRNG generator)
      : impl_(generator),
        table_name_("ReadExperiment_" + Traits::TableSuffix()) {}

  Status SetUp(Config const& config, cs::Database const& database) override {
    return impl_.FillTable(config, database, table_name_);
  }

  Status TearDown(Config const&, cs::Database const&) override { return {}; }

  Status Run(Config const& config, cs::Database const& database) override {
    // Create enough clients and stubs for the worst case
    std::vector<cs::Client> clients;
    std::vector<std::shared_ptr<cs::internal::SpannerStub>> stubs;
    std::cout << "# Creating clients and stubs " << std::flush;
    for (int i = 0; i != config.maximum_clients; ++i) {
      auto options = cs::ConnectionOptions().set_channel_pool_domain(
          "task:" + std::to_string(i));
      clients.emplace_back(cs::Client(cs::MakeConnection(database, options)));
      stubs.emplace_back(
          cs::internal::CreateDefaultSpannerStub(options, /*channel_id=*/0));
      std::cout << '.' << std::flush;
    }
    std::cout << " DONE\n";

    // Capture some overall getrusage() statistics as comments.
    SimpleTimer overall;
    overall.Start();
    for (int i = 0; i != config.samples; ++i) {
      auto const use_stubs = impl_.UseStub(config);
      auto const thread_count = impl_.ThreadCount(config);
      auto const client_count = impl_.ClientCount(config, thread_count);
      if (use_stubs) {
        std::vector<std::shared_ptr<cs::internal::SpannerStub>> iteration_stubs(
            stubs.begin(), stubs.begin() + client_count);
        RunIterationViaStubs(config, iteration_stubs, thread_count);
        continue;
      }
      std::vector<cs::Client> iteration_clients(clients.begin(),
                                                clients.begin() + client_count);
      RunIterationViaClients(config, iteration_clients, thread_count);
    }
    overall.Stop();
    std::cout << overall.annotations();
    return {};
  }

 private:
  void RunIterationViaStubs(
      Config const& config,
      std::vector<std::shared_ptr<cs::internal::SpannerStub>> const& stubs,
      int thread_count) {
    std::vector<std::future<std::vector<RowCpuSample>>> tasks(thread_count);
    int task_id = 0;
    for (auto& t : tasks) {
      auto client = stubs[task_id++ % stubs.size()];
      t = std::async(std::launch::async, &ReadExperiment::ReadRowsViaStub, this,
                     config, thread_count, static_cast<int>(stubs.size()),
                     cs::Database(config.project_id, config.instance_id,
                                  config.database_id),
                     client);
    }
    for (auto& t : tasks) {
      impl_.DumpSamples(t.get());
    }
  }

  std::vector<RowCpuSample> ReadRowsViaStub(
      Config const& config, int thread_count, int client_count,
      cs::Database const& database,
      std::shared_ptr<cs::internal::SpannerStub> const& stub) {
    auto session = [&]() -> google::cloud::StatusOr<std::string> {
      Status last_status;
      for (int i = 0; i != 10; ++i) {
        grpc::ClientContext context;
        google::spanner::v1::CreateSessionRequest request{};
        request.set_database(database.FullName());
        auto response = stub->CreateSession(context, request);
        if (response) return response->name();
        last_status = response.status();
      }
      return last_status;
    }();

    if (!session) {
      std::ostringstream os;
      os << "SESSION ERROR = " << session.status();
      impl_.LogError(os.str());
      return {};
    }

    std::vector<RowCpuSample> samples;
    // We expect about 50 reads per second per thread. Use that to estimate
    // the size of the vector.
    samples.reserve(config.iteration_duration.count() * 50);
    std::vector<std::string> const columns{"Key",   "Data0", "Data1", "Data2",
                                           "Data3", "Data4", "Data5", "Data6",
                                           "Data7", "Data8", "Data9"};
    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.iteration_duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto key = impl_.RandomKeySet(config);

      SimpleTimer timer;
      timer.Start();

      google::spanner::v1::ReadRequest request{};
      request.set_session(*session);
      request.mutable_transaction()
          ->mutable_single_use()
          ->mutable_read_only()
          ->Clear();
      request.set_table(table_name_);
      for (auto const& name : columns) {
        request.add_columns(name);
      }
      *request.mutable_key_set() = cs::internal::ToProto(key);

      int row_count = 0;
      google::spanner::v1::PartialResultSet result;
      std::vector<google::protobuf::Value> row;
      grpc::ClientContext context;
      auto stream = stub->StreamingRead(context, request);
      for (bool success = stream->Read(&result); success;
           success = stream->Read(&result)) {
        if (result.chunked_value()) {
          // We do not handle chunked values in the benchmark.
          continue;
        }
        row.resize(columns.size());
        std::size_t index = 0;
        for (auto& value : *result.mutable_values()) {
          row[index] = std::move(value);
          if (++index == columns.size()) {
            ++row_count;
            index = 0;
          }
        }
      }
      auto final = stream->Finish();
      timer.Stop();
      samples.push_back(RowCpuSample{
          thread_count, client_count, true, row_count, timer.elapsed_time(),
          timer.cpu_time(),
          google::cloud::grpc_utils::MakeStatusFromRpcError(final)});
    }
    return samples;
  }

  void RunIterationViaClients(Config const& config,
                              std::vector<cs::Client> const& clients,
                              int thread_count) {
    std::vector<std::future<std::vector<RowCpuSample>>> tasks(thread_count);
    int task_id = 0;
    for (auto& t : tasks) {
      auto client = clients[task_id++ % clients.size()];
      t = std::async(std::launch::async, &ReadExperiment::ReadRowsViaClients,
                     this, config, thread_count,
                     static_cast<int>(clients.size()), client);
    }
    for (auto& t : tasks) {
      impl_.DumpSamples(t.get());
    }
  }

  std::vector<RowCpuSample> ReadRowsViaClients(Config const& config,
                                               int thread_count,
                                               int client_count,
                                               cs::Client client) {
    std::vector<std::string> const column_names{
        "Key",   "Data0", "Data1", "Data2", "Data3", "Data4",
        "Data5", "Data6", "Data7", "Data8", "Data9"};

    using T = typename Traits::native_type;
    using RowType = std::tuple<std::int64_t, T, T, T, T, T, T, T, T, T, T>;
    std::vector<RowCpuSample> samples;
    // We expect about 50 reads per second per thread, so allocate enough
    // memory to start.
    samples.reserve(config.iteration_duration.count() * 50);
    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.iteration_duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto key = impl_.RandomKeySet(config);

      SimpleTimer timer;
      timer.Start();
      auto rows = client.Read(table_name_, key, column_names);
      int row_count = 0;
      Status status;
      for (auto& row : cs::StreamOf<RowType>(rows)) {
        if (!row) {
          status = std::move(row).status();
          break;
        }
        ++row_count;
      }
      timer.Stop();
      samples.push_back(RowCpuSample{thread_count, client_count, false,
                                     row_count, timer.elapsed_time(),
                                     timer.cpu_time(), std::move(status)});
    }
    return samples;
  }

  ExperimentImpl<Traits> impl_;
  std::string table_name_;
};

/**
 * Run an experiment to measure the CPU overhead of the client over raw gRPC.
 *
 * This experiments creates and populates a table with K rows, each row
 * containing an (integer) key and 10 columns of the types defined by `Traits`.
 * Then the experiment performs M iterations of:
 *   - Randomly select if it will read using the client library or raw gRPC.
 *   - Then for N seconds update random rows
 *   - Measure the CPU time required by the previous step
 *
 * The values of K, M, N are configurable.
 */
template <typename Traits>
class UpdateExperiment : public Experiment {
 public:
  explicit UpdateExperiment(
      google::cloud::internal::DefaultPRNG const& generator)
      : impl_(generator),
        table_name_("UpdateExperiment_" + Traits::TableSuffix()) {}

  Status SetUp(Config const& config, cs::Database const& database) override {
    return impl_.FillTable(config, database, table_name_);
  }

  Status TearDown(Config const&, cs::Database const&) override { return {}; }

  Status Run(Config const& config, cs::Database const& database) override {
    // Create enough clients and stubs for the worst case
    std::vector<cs::Client> clients;
    std::vector<std::shared_ptr<cs::internal::SpannerStub>> stubs;
    std::cout << "# Creating clients and stubs " << std::flush;
    for (int i = 0; i != config.maximum_clients; ++i) {
      auto options = cs::ConnectionOptions().set_channel_pool_domain(
          "task:" + std::to_string(i));
      clients.emplace_back(cs::Client(cs::MakeConnection(database, options)));
      stubs.emplace_back(
          cs::internal::CreateDefaultSpannerStub(options, /*channel_id=*/0));
      std::cout << '.' << std::flush;
    }
    std::cout << " DONE\n";

    // Capture some overall getrusage() statistics as comments.
    SimpleTimer overall;
    overall.Start();
    for (int i = 0; i != config.samples; ++i) {
      auto const use_stubs = impl_.UseStub(config);
      auto const thread_count = impl_.ThreadCount(config);
      auto const client_count = impl_.ClientCount(config, thread_count);
      if (use_stubs) {
        std::vector<std::shared_ptr<cs::internal::SpannerStub>> iteration_stubs(
            stubs.begin(), stubs.begin() + client_count);
        RunIterationViaStubs(config, iteration_stubs, thread_count);
        continue;
      }
      std::vector<cs::Client> iteration_clients(clients.begin(),
                                                clients.begin() + client_count);
      RunIterationViaClients(config, iteration_clients, thread_count);
    }
    overall.Stop();
    std::cout << overall.annotations();
    return {};
  }

 private:
  void RunIterationViaStubs(
      Config const& config,
      std::vector<std::shared_ptr<cs::internal::SpannerStub>> const& stubs,
      int thread_count) {
    std::vector<std::future<std::vector<RowCpuSample>>> tasks(thread_count);
    int task_id = 0;
    for (auto& t : tasks) {
      auto client = stubs[task_id++ % stubs.size()];
      t = std::async(std::launch::async, &UpdateExperiment::UpdateRowsViaStub,
                     this, config, thread_count, static_cast<int>(stubs.size()),
                     cs::Database(config.project_id, config.instance_id,
                                  config.database_id),
                     client);
    }
    for (auto& t : tasks) {
      impl_.DumpSamples(t.get());
    }
  }

  std::vector<RowCpuSample> UpdateRowsViaStub(
      Config const& config, int thread_count, int client_count,
      cs::Database const& database,
      std::shared_ptr<cs::internal::SpannerStub> const& stub) {
    std::string const statement = CreateStatement();

    auto session = [&]() -> google::cloud::StatusOr<std::string> {
      Status last_status;
      for (int i = 0; i != 10; ++i) {
        grpc::ClientContext context;
        google::spanner::v1::CreateSessionRequest request{};
        request.set_database(database.FullName());
        auto response = stub->CreateSession(context, request);
        if (response) return response->name();
        last_status = response.status();
      }
      return last_status;
    }();

    if (!session) {
      std::ostringstream os;
      os << "SESSION ERROR = " << session.status();
      impl_.LogError(os.str());
      return {};
    }

    std::vector<RowCpuSample> samples;
    // We expect about 50 reads per second per thread. Use that to estimate
    // the size of the vector.
    samples.reserve(config.iteration_duration.count() * 50);
    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.iteration_duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto const key = impl_.RandomKey(config);

      using T = typename Traits::native_type;
      std::vector<T> const values{
          impl_.GenerateRandomValue(), impl_.GenerateRandomValue(),
          impl_.GenerateRandomValue(), impl_.GenerateRandomValue(),
          impl_.GenerateRandomValue(), impl_.GenerateRandomValue(),
          impl_.GenerateRandomValue(), impl_.GenerateRandomValue(),
          impl_.GenerateRandomValue(), impl_.GenerateRandomValue(),
      };

      SimpleTimer timer;
      timer.Start();

      google::spanner::v1::ExecuteSqlRequest request{};
      request.set_session(*session);
      request.mutable_transaction()
          ->mutable_begin()
          ->mutable_read_write()
          ->Clear();
      request.set_sql(statement);
      auto key_type_value = cs::internal::ToProto(cs::Value(key));
      (*request.mutable_param_types())["key"] = std::move(key_type_value.first);
      (*request.mutable_params()->mutable_fields())["key"] =
          std::move(key_type_value.second);

      for (int i = 0; i != 10; ++i) {
        auto tv = cs::internal::ToProto(cs::Value(values[i]));
        auto name = "v" + std::to_string(i);
        (*request.mutable_param_types())[name] = std::move(tv.first);
        (*request.mutable_params()->mutable_fields())[name] =
            std::move(tv.second);
      }

      int row_count = 0;
      std::string transaction_id;
      google::cloud::Status status;
      {
        grpc::ClientContext context;
        auto response = stub->ExecuteSql(context, request);
        if (response) {
          row_count =
              static_cast<int>(response->stats().row_count_lower_bound());
          transaction_id = response->metadata().transaction().id();
        } else {
          status = std::move(response).status();
        }
      }

      if (status.ok()) {
        grpc::ClientContext context;
        google::spanner::v1::CommitRequest commit_request;
        commit_request.set_session(*session);
        commit_request.set_transaction_id(transaction_id);
        auto response = stub->Commit(context, commit_request);
        if (!response) status = std::move(response).status();
      }

      timer.Stop();
      samples.push_back(RowCpuSample{thread_count, client_count, true,
                                     row_count, timer.elapsed_time(),
                                     timer.cpu_time(), status});
    }
    return samples;
  }

  void RunIterationViaClients(Config const& config,
                              std::vector<cs::Client> const& clients,
                              int thread_count) {
    std::vector<std::future<std::vector<RowCpuSample>>> tasks(thread_count);
    int task_id = 0;
    for (auto& t : tasks) {
      auto client = clients[task_id++ % clients.size()];
      t = std::async(std::launch::async, &UpdateExperiment::UpdateRowsViaClient,
                     this, config, thread_count,
                     static_cast<int>(clients.size()), client);
    }
    for (auto& t : tasks) {
      impl_.DumpSamples(t.get());
    }
  }

  std::vector<RowCpuSample> UpdateRowsViaClient(Config const& config,
                                                int thread_count,
                                                int client_count,
                                                cs::Client client) {
    std::string const statement = CreateStatement();

    std::vector<RowCpuSample> samples;
    // We expect about 50 reads per second per thread, so allocate enough
    // memory to start.
    samples.reserve(config.iteration_duration.count() * 50);
    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.iteration_duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto const key = impl_.RandomKey(config);

      using T = typename Traits::native_type;
      std::vector<T> const values{
          impl_.GenerateRandomValue(), impl_.GenerateRandomValue(),
          impl_.GenerateRandomValue(), impl_.GenerateRandomValue(),
          impl_.GenerateRandomValue(), impl_.GenerateRandomValue(),
          impl_.GenerateRandomValue(), impl_.GenerateRandomValue(),
          impl_.GenerateRandomValue(), impl_.GenerateRandomValue(),
      };

      SimpleTimer timer;
      timer.Start();
      std::unordered_map<std::string, cs::Value> const params{
          {"key", cs::Value(key)},      {"v0", cs::Value(values[0])},
          {"v1", cs::Value(values[1])}, {"v2", cs::Value(values[2])},
          {"v3", cs::Value(values[3])}, {"v4", cs::Value(values[4])},
          {"v5", cs::Value(values[5])}, {"v6", cs::Value(values[6])},
          {"v7", cs::Value(values[7])}, {"v8", cs::Value(values[8])},
          {"v9", cs::Value(values[9])},
      };

      int row_count = 0;
      auto commit_result =
          client.Commit([&](cs::Transaction const& txn)
                            -> google::cloud::StatusOr<cs::Mutations> {
            auto result =
                client.ExecuteDml(txn, cs::SqlStatement(statement, params));
            if (!result) return std::move(result).status();
            row_count = static_cast<int>(result->RowsModified());
            return cs::Mutations{};
          });
      timer.Stop();
      samples.push_back(RowCpuSample{
          thread_count, client_count, false, row_count, timer.elapsed_time(),
          timer.cpu_time(), std::move(commit_result).status()});
    }
    return samples;
  }

  std::string CreateStatement() const {
    std::string sql = "UPDATE " + table_name_;
    char const* sep = " SET ";
    for (int i = 0; i != 10; ++i) {
      sql += sep;
      sql += " Data" + std::to_string(i) + " = @v" + std::to_string(i);
      sep = ", ";
    }
    sql += " WHERE Key = @key";
    return sql;
  }

  ExperimentImpl<Traits> impl_;
  std::string table_name_;
};

class RunAllExperiment : public Experiment {
 public:
  explicit RunAllExperiment(google::cloud::internal::DefaultPRNG generator)
      : generator_(generator) {}
  Status SetUp(Config const&, cs::Database const&) override { return {}; }
  Status TearDown(Config const&, cs::Database const&) override { return {}; }

  Status Run(Config const& cfg, cs::Database const& /*database*/) override {
    // Smoke test all the experiments by running a very small version of each.

    std::vector<std::future<google::cloud::Status>> tasks;
    for (auto& kv : AvailableExperiments()) {
      // Do not recurse, skip this experiment.
      if (kv.first == "run-all") continue;
      Config config = cfg;
      config.experiment = kv.first;
      config.samples = 1;
      config.iteration_duration = std::chrono::seconds(1);
      config.minimum_threads = 1;
      config.maximum_threads = 1;
      config.minimum_clients = 1;
      config.maximum_clients = 1;
      config.table_size = 10;
      config.query_size = 1;

      auto experiment = kv.second(generator_);

      // TODO(#1119) - tests disabled until we can stay within admin op quota
#if 0
      tasks.push_back(std::async(
          std::launch::async,
          [](Config config, cs::Database const& database,
             std::mutex& mu, std::unique_ptr<Experiment> experiment) {
            {
              std::lock_guard<std::mutex> lk(mu);
              std::cout << "# Smoke test for experiment\n";
              std::cout << config << "\n" << std::flush;
            }
            auto status = experiment->SetUp(config, database);
            if (!status.ok()) {
              std::lock_guard<std::mutex> lk(mu);
              std::cout << "# ERROR in SetUp: " << status << "\n";
              return status;
            }
            config.use_only_clients = true;
            experiment->Run(config, database);
            config.use_only_stubs = true;
            experiment->Run(config, database);
            experiment->TearDown(config, database);
            return google::cloud::Status();
          },
          config, database, std::ref(mu_), std::move(experiment)));
#endif
    }

    Status status;
    for (auto& task : tasks) {
      auto s = task.get();
      if (!s.ok()) {
        status = std::move(s);
      }
    }
    return status;
  }

 private:
  std::mutex mu_;
  google::cloud::internal::DefaultPRNG generator_;
};

template <typename Trait>
ExperimentFactory MakeReadFactory() {
  using G = google::cloud::internal::DefaultPRNG;
  return [](G g) {
    return google::cloud::internal::make_unique<ReadExperiment<Trait>>(g);
  };
}

template <typename Trait>
ExperimentFactory MakeUpdateFactory() {
  using G = google::cloud::internal::DefaultPRNG;
  return [](G g) {
    return google::cloud::internal::make_unique<UpdateExperiment<Trait>>(g);
  };
}

std::map<std::string, ExperimentFactory> AvailableExperiments() {
  auto make_run_all = [](google::cloud::internal::DefaultPRNG g) {
    return google::cloud::internal::make_unique<RunAllExperiment>(g);
  };

  return {
      {"run-all", make_run_all},
      {"read-bool", MakeReadFactory<BoolTraits>()},
      {"read-bytes", MakeReadFactory<BytesTraits>()},
      {"read-date", MakeReadFactory<DateTraits>()},
      {"read-float64", MakeReadFactory<Float64Traits>()},
      {"read-int64", MakeReadFactory<Int64Traits>()},
      {"read-string", MakeReadFactory<StringTraits>()},
      {"read-timestamp", MakeReadFactory<TimestampTraits>()},
      {"update-bool", MakeUpdateFactory<BoolTraits>()},
      {"update-bytes", MakeUpdateFactory<BytesTraits>()},
      {"update-date", MakeUpdateFactory<DateTraits>()},
      {"update-float64", MakeUpdateFactory<Float64Traits>()},
      {"update-int64", MakeUpdateFactory<Int64Traits>()},
      {"update-string", MakeUpdateFactory<StringTraits>()},
      {"update-timestamp", MakeUpdateFactory<TimestampTraits>()},
  };
}

google::cloud::StatusOr<Config> ParseArgs(std::vector<std::string> args) {
  Config config;

  config.experiment = "run-all";

  config.project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  config.instance_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_SPANNER_INSTANCE")
          .value_or("");

  struct Flag {
    std::string flag_name;
    std::function<void(Config&, std::string)> parser;
  };

  // NOLINTNEXTLINE(modernize-avoid-c-arrays)
  Flag flags[] = {
      {"--experiment=",
       [](Config& c, std::string v) { c.experiment = std::move(v); }},
      {"--project=",
       [](Config& c, std::string v) { c.project_id = std::move(v); }},
      {"--instance=",
       [](Config& c, std::string v) { c.instance_id = std::move(v); }},
      {"--samples=",
       [](Config& c, std::string const& v) { c.samples = std::stoi(v); }},
      {"--iteration-duration=",
       [](Config& c, std::string const& v) {
         c.iteration_duration = std::chrono::seconds(std::stoi(v));
       }},
      {"--minimum-threads=",
       [](Config& c, std::string const& v) {
         c.minimum_threads = std::stoi(v);
       }},
      {"--maximum-threads=",
       [](Config& c, std::string const& v) {
         c.maximum_threads = std::stoi(v);
       }},
      {"--minimum-clients=",
       [](Config& c, std::string const& v) {
         c.minimum_clients = std::stoi(v);
       }},
      {"--maximum-clients=",
       [](Config& c, std::string const& v) {
         c.maximum_clients = std::stoi(v);
       }},
      {"--table-size=",
       [](Config& c, std::string const& v) { c.table_size = std::stol(v); }},
      {"--query-size=",
       [](Config& c, std::string const& v) { c.query_size = std::stol(v); }},
      {"--use-only-stubs",
       [](Config& c, std::string const&) { c.use_only_stubs = true; }},
      {"--use-only-clients",
       [](Config& c, std::string const&) { c.use_only_clients = true; }},
  };

  auto invalid_argument = [](std::string msg) {
    return google::cloud::Status(google::cloud::StatusCode::kInvalidArgument,
                                 std::move(msg));
  };

  for (auto i = std::next(args.begin()); i != args.end(); ++i) {
    std::string const& arg = *i;
    bool found = false;
    for (auto const& flag : flags) {
      if (arg.rfind(flag.flag_name, 0) != 0) continue;
      found = true;
      flag.parser(config, arg.substr(flag.flag_name.size()));

      break;
    }
    if (!found && arg.rfind("--", 0) == 0) {
      return invalid_argument("Unexpected command-line flag " + arg);
    }
  }

  if (config.experiment.empty()) {
    return invalid_argument("Missing value for --experiment flag");
  }

  if (config.project_id.empty()) {
    return invalid_argument(
        "The project id is not set, provide a value in the --project flag,"
        " or set the GOOGLE_CLOUD_PROJECT environment variable");
  }

  if (config.minimum_threads <= 0) {
    std::ostringstream os;
    os << "The minimum number of threads (" << config.minimum_threads << ")"
       << " must be greater than zero";
    return invalid_argument(os.str());
  }
  if (config.maximum_threads < config.minimum_threads) {
    std::ostringstream os;
    os << "The maximum number of threads (" << config.maximum_threads << ")"
       << " must be greater or equal than the minimum number of threads ("
       << config.minimum_threads << ")";
    return invalid_argument(os.str());
  }

  if (!SupportPerThreadUsage() && config.maximum_threads > 1) {
    std::ostringstream os;
    os << "Your platform does not support per-thread getrusage() data."
       << " The benchmark cannot run with more than one thread, and you"
       << " set maximum threads to " << config.maximum_threads;
    return invalid_argument(os.str());
  }

  if (config.minimum_clients <= 0) {
    std::ostringstream os;
    os << "The minimum number of clients (" << config.minimum_clients << ")"
       << " must be greater than zero";
    return invalid_argument(os.str());
  }
  if (config.maximum_clients < config.minimum_clients) {
    std::ostringstream os;
    os << "The maximum number of clients (" << config.maximum_clients << ")"
       << " must be greater or equal than the minimum number of clients ("
       << config.minimum_clients << ")";
    return invalid_argument(os.str());
  }

  if (config.query_size <= 0) {
    std::ostringstream os;
    os << "The query size (" << config.query_size << ") should be > 0";
    return invalid_argument(os.str());
  }

  if (config.table_size < config.query_size) {
    std::ostringstream os;
    os << "The table size (" << config.table_size << ") should be greater"
       << " than the query size (" << config.query_size << ")";
    return invalid_argument(os.str());
  }

  if (config.use_only_stubs && config.use_only_clients) {
    std::ostringstream os;
    os << "Only one of --use-only-stubs or --use-only-clients can be set";
    return invalid_argument(os.str());
  }

  return config;
}

#if GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
namespace {
int RUsageWho() {
#if GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD
  return RUSAGE_THREAD;
#else
  return RUSAGE_SELF;
#endif  // GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD
}
}  // namespace
#endif  // GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE

void SimpleTimer::Start() {
#if GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
  (void)getrusage(RUsageWho(), &start_usage_);
#endif  // GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
  start_ = std::chrono::steady_clock::now();
}

void SimpleTimer::Stop() {
  using std::chrono::duration_cast;
  using std::chrono::microseconds;
  using std::chrono::seconds;
  using std::chrono::steady_clock;
  elapsed_time_ = duration_cast<microseconds>(steady_clock::now() - start_);

#if GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
  auto as_usec = [](timeval const& tv) {
    return microseconds(seconds(tv.tv_sec)) + microseconds(tv.tv_usec);
  };

  struct rusage now {};
  (void)getrusage(RUsageWho(), &now);
  auto utime = as_usec(now.ru_utime) - as_usec(start_usage_.ru_utime);
  auto stime = as_usec(now.ru_stime) - as_usec(start_usage_.ru_stime);
  cpu_time_ = utime + stime;
  double cpu_fraction = 0;
  if (elapsed_time_.count() != 0) {
    cpu_fraction =
        (cpu_time_).count() / static_cast<double>(elapsed_time_.count());
  }
  now.ru_minflt -= start_usage_.ru_minflt;
  now.ru_majflt -= start_usage_.ru_majflt;
  now.ru_nswap -= start_usage_.ru_nswap;
  now.ru_inblock -= start_usage_.ru_inblock;
  now.ru_oublock -= start_usage_.ru_oublock;
  now.ru_msgsnd -= start_usage_.ru_msgsnd;
  now.ru_msgrcv -= start_usage_.ru_msgrcv;
  now.ru_nsignals -= start_usage_.ru_nsignals;
  now.ru_nvcsw -= start_usage_.ru_nvcsw;
  now.ru_nivcsw -= start_usage_.ru_nivcsw;

  std::ostringstream os;
  os << "# user time                    =" << utime.count() << " us\n"
     << "# system time                  =" << stime.count() << " us\n"
     << "# CPU fraction                 =" << cpu_fraction << "\n"
     << "# maximum resident set size    =" << now.ru_maxrss << " KiB\n"
     << "# integral shared memory size  =" << now.ru_ixrss << " KiB\n"
     << "# integral unshared data size  =" << now.ru_idrss << " KiB\n"
     << "# integral unshared stack size =" << now.ru_isrss << " KiB\n"
     << "# soft page faults             =" << now.ru_minflt << "\n"
     << "# hard page faults             =" << now.ru_majflt << "\n"
     << "# swaps                        =" << now.ru_nswap << "\n"
     << "# block input operations       =" << now.ru_inblock << "\n"
     << "# block output operations      =" << now.ru_oublock << "\n"
     << "# IPC messages sent            =" << now.ru_msgsnd << "\n"
     << "# IPC messages received        =" << now.ru_msgrcv << "\n"
     << "# signals received             =" << now.ru_nsignals << "\n"
     << "# voluntary context switches   =" << now.ru_nvcsw << "\n"
     << "# involuntary context switches =" << now.ru_nivcsw << "\n";
  annotations_ = std::move(os).str();
#endif  // GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
}

bool SupportPerThreadUsage() {
#if GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD
  return true;
#else
  return false;
#endif  // GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD
}

}  // namespace
