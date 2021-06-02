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

#include "google/cloud/spanner/benchmarks/benchmarks_config.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/internal/defaults.h"
#include "google/cloud/spanner/internal/session_pool.h"
#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/timer.h"
#include "absl/memory/memory.h"
#include "absl/time/civil_time.h"
#include <google/spanner/v1/result_set.pb.h>
#include <grpcpp/grpcpp.h>
#include <algorithm>
#include <chrono>
#include <future>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <thread>

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

namespace {

namespace spanner = ::google::cloud::spanner;
namespace spanner_internal = ::google::cloud::spanner_internal;
using ::google::cloud::Status;
using ::google::cloud::spanner_benchmarks::Config;
using ::google::cloud::testing_util::Timer;

struct RowCpuSample {
  int channel_count;
  int thread_count;
  bool using_stub;
  int row_count;
  std::chrono::microseconds elapsed;
  std::chrono::microseconds cpu_time;
  Status status;
};

std::ostream& operator<<(std::ostream& os, RowCpuSample const& s) {
  return os << s.channel_count << ',' << s.thread_count << ',' << s.using_stub
            << ',' << s.row_count << ',' << s.elapsed.count() << ','
            << s.cpu_time.count() << ',' << s.status.code();
}

bool SupportPerThreadUsage() {
#if GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD
  return true;
#else
  return false;
#endif  // GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD
}

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
  using native_type = spanner::Bytes;
  static std::string SpannerDataType() { return "BYTES(1024)"; }
  static std::string TableSuffix() { return "bytes"; }
  static native_type MakeRandomValue(
      google::cloud::internal::DefaultPRNG& generator) {
    static std::string const kPopulation = [] {
      std::string result;
      // NOLINTNEXTLINE(bugprone-signed-char-misuse)
      int constexpr kCharMin = (std::numeric_limits<char>::min)();
      int constexpr kCharMax = (std::numeric_limits<char>::max)();
      for (auto c = kCharMin; c <= kCharMax; ++c) {
        result.push_back(static_cast<char>(c));
      }
      return result;
    }();
    std::string tmp =
        google::cloud::internal::Sample(generator, 1024, kPopulation);
    return spanner::Bytes(tmp.begin(), tmp.end());
  }
};

struct DateTraits {
  using native_type = absl::CivilDay;
  static std::string SpannerDataType() { return "DATE"; }
  static std::string TableSuffix() { return "date"; }
  static native_type MakeRandomValue(
      google::cloud::internal::DefaultPRNG& generator) {
    return native_type{
        std::uniform_int_distribution<std::int64_t>(1, 2000)(generator),
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
  using native_type = spanner::Timestamp;
  static std::string SpannerDataType() { return "TIMESTAMP"; }
  static std::string TableSuffix() { return "timestamp"; }
  static native_type MakeRandomValue(
      google::cloud::internal::DefaultPRNG& generator) {
    auto const tp =
        std::chrono::system_clock::time_point{} +
        std::chrono::nanoseconds(
            std::uniform_int_distribution<std::chrono::nanoseconds::rep>(
                0, std::numeric_limits<std::chrono::nanoseconds::rep>::max())(
                generator));
    return spanner::MakeTimestamp(tp).value();
  }
};

struct NumericTraits {
  using native_type = spanner::Numeric;
  static std::string SpannerDataType() { return "NUMERIC"; }
  static std::string TableSuffix() { return "numeric"; }
  static native_type MakeRandomValue(
      google::cloud::internal::DefaultPRNG& generator) {
    return spanner::MakeNumeric(
               std::uniform_int_distribution<std::int64_t>(
                   std::numeric_limits<std::int64_t>::min(),
                   std::numeric_limits<std::int64_t>::max())(generator),
               -9)  // scale by 10^-9
        .value();
  }
};

template <typename Traits>
class ExperimentImpl {
 public:
  explicit ExperimentImpl(google::cloud::internal::DefaultPRNG const& generator)
      : generator_(generator) {}

  static int constexpr kColumnCount = 10;

  std::string CreateTableStatement(std::string const& table_name) {
    std::string statement = "CREATE TABLE " + table_name;
    statement += " (Key INT64 NOT NULL,\n";
    for (int i = 0; i != kColumnCount; ++i) {
      statement +=
          "Data" + std::to_string(i) + " " + Traits::SpannerDataType() + ",\n";
    }
    statement += ") PRIMARY KEY (Key)";
    return statement;
  }

  Status FillTable(Config const& config, spanner::Database const& database,
                   std::string const& table_name) {
    // We need to populate some data or all the requests to read will fail.
    spanner::Client client(spanner::MakeConnection(database));
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

  std::int64_t RandomKeySetBegin(Config const& config) {
    std::lock_guard<std::mutex> lk(mu_);
    return std::uniform_int_distribution<std::int64_t>(
        0, config.table_size - config.query_size)(generator_);
  }

  spanner::KeySet RandomKeySet(Config const& config) {
    auto begin = RandomKeySetBegin(config);
    auto end = begin + config.query_size - 1;
    return spanner::KeySet().AddRange(
        spanner::MakeKeyBoundClosed(spanner::Value(begin)),
        spanner::MakeKeyBoundClosed(spanner::Value(end)));
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

  int ChannelCount(Config const& config) {
    std::lock_guard<std::mutex> lk(mu_);
    return std::uniform_int_distribution<int>(
        config.minimum_channels, config.maximum_channels)(generator_);
  }

  spanner::Client MakeClient(Config const& config,
                             spanner::Database const& database) {
    auto num_channels = config.maximum_channels;
    std::cout << "# Creating 1 client using shared connection with "
              << num_channels << " channel" << (num_channels != 1 ? "s" : "")
              << "\n"
              << std::flush;

    auto connection = spanner::MakeConnection(
        database, spanner::ConnectionOptions().set_num_channels(num_channels),
        // This pre-creates all the Sessions we will need (one per thread).
        spanner::SessionPoolOptions().set_min_sessions(config.maximum_threads));
    return spanner::Client(std::move(connection));
  }

  std::vector<std::shared_ptr<spanner_internal::SpannerStub>> MakeStubs(
      Config const& config, spanner::Database const& db) {
    auto num_channels = config.maximum_channels;
    std::cout << "# Creating " << num_channels << " stub"
              << (num_channels != 1 ? "s" : "") << "\n"
              << std::flush;
    auto opts = google::cloud::internal::MakeOptions(
        spanner::ConnectionOptions().set_num_channels(num_channels));
    opts = spanner_internal::DefaultOptions(std::move(opts));
    std::vector<std::shared_ptr<spanner_internal::SpannerStub>> stubs;
    stubs.reserve(num_channels);
    for (int channel_id = 0; channel_id < num_channels; ++channel_id) {
      stubs.push_back(
          spanner_internal::CreateDefaultSpannerStub(db, opts, channel_id));
    }
    return stubs;
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
  Status FillTableTask(Config const& config, spanner::Client client,
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

    auto mutation =
        spanner::InsertOrUpdateMutationBuilder(table_name, column_names);
    int current_mutations = 0;

    auto maybe_flush = [&](bool force) -> Status {
      if (current_mutations == 0) {
        return {};
      }
      if (!force && current_mutations < 1000) {
        return {};
      }
      auto result =
          client.Commit(spanner::Mutations{std::move(mutation).Build()});
      if (!result) {
        std::lock_guard<std::mutex> lk(mu_);
        std::cout << "# Error in Commit() " << result.status() << "\n";
        return std::move(result).status();
      }
      mutation =
          spanner::InsertOrUpdateMutationBuilder(table_name, column_names);
      current_mutations = 0;
      return {};
    };
    auto force_flush = [&maybe_flush] { return maybe_flush(true); };
    auto flush_as_needed = [&maybe_flush] { return maybe_flush(false); };

    auto const report_period =
        (std::max)(static_cast<std::int32_t>(2), config.table_size / 50);
    for (std::int64_t key = 0; key != config.table_size; ++key) {
      // Each thread does a fraction of the key space.
      if (key % task_count != task_id) continue;
      // Have one of the threads report progress about 50 times.
      if (task_id == 0 && key % report_period == 0) {
        std::lock_guard<std::mutex> lk(mu_);
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

class Experiment {
 public:
  virtual ~Experiment() = default;

  virtual std::string AdditionalDdlStatement() = 0;
  virtual Status SetUp(Config const& config,
                       spanner::Database const& database) = 0;
  virtual Status Run(Config const& config,
                     spanner::Database const& database) = 0;
};

template <typename Traits>
class BasicExperiment : public Experiment {
 public:
  explicit BasicExperiment(google::cloud::internal::DefaultPRNG generator,
                           std::string table_prefix)
      : impl_(generator),
        table_name_(absl::StrCat(std::move(table_prefix), "Experiment_",
                                 Traits::TableSuffix())) {}

  std::string AdditionalDdlStatement() override {
    return impl_.CreateTableStatement(table_name_);
  }

  Status SetUp(Config const& config,
               spanner::Database const& database) override {
    return impl_.FillTable(config, database, table_name_);
  }

  Status Run(Config const& config, spanner::Database const& database) override {
    auto client = impl_.MakeClient(config, database);
    auto stubs = impl_.MakeStubs(config, database);

    // Capture some overall getrusage() statistics as comments.
    Timer overall;
    overall.Start();
    for (int i = 0; i != config.samples; ++i) {
      auto const use_stubs = impl_.UseStub(config);
      auto const thread_count = impl_.ThreadCount(config);
      auto const channel_count = impl_.ChannelCount(config);
      if (use_stubs) {
        std::vector<std::shared_ptr<spanner_internal::SpannerStub>>
            iteration_stubs(stubs.begin(), stubs.begin() + channel_count);
        RunIterationViaStubs(config, iteration_stubs, thread_count);
        continue;
      }
      RunIterationViaClient(config, client, thread_count, channel_count);
    }
    overall.Stop();
    std::cout << overall.annotations();
    return {};
  }

 protected:
  virtual std::vector<RowCpuSample> ViaStub(
      Config const& config, int thread_count, int channel_count,
      spanner::Database const& database,
      std::shared_ptr<spanner_internal::SpannerStub> stub) = 0;

  virtual std::vector<RowCpuSample> ViaClient(Config const& config,
                                              int thread_count,
                                              int channel_count,
                                              spanner::Client client) = 0;

 private:
  void RunIterationViaStubs(
      Config const& config,
      std::vector<std::shared_ptr<spanner_internal::SpannerStub>> const& stubs,
      int thread_count) {
    std::vector<std::future<std::vector<RowCpuSample>>> tasks(thread_count);
    int num_stubs = static_cast<int>(stubs.size());
    int task_id = 0;
    for (auto& t : tasks) {
      auto client = stubs[task_id++ % num_stubs];
      t = std::async(std::launch::async, [this, &config, thread_count,
                                          num_stubs, client] {
        return ViaStub(config, thread_count, num_stubs,
                       spanner::Database(config.project_id, config.instance_id,
                                         config.database_id),
                       client);
      });
    }
    for (auto& t : tasks) {
      impl_.DumpSamples(t.get());
    }
  }

  void RunIterationViaClient(Config const& config,
                             spanner::Client const& client, int thread_count,
                             int channel_count) {
    std::vector<std::future<std::vector<RowCpuSample>>> tasks(thread_count);
    for (auto& t : tasks) {
      t = std::async(std::launch::async, [this, &config, &thread_count,
                                          &channel_count, &client] {
        return ViaClient(config, thread_count, channel_count, client);
      });
    }
    for (auto& t : tasks) {
      impl_.DumpSamples(t.get());
    }
  }

 protected:
  ExperimentImpl<Traits> impl_;
  std::string table_name_;
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
class ReadExperiment : public BasicExperiment<Traits> {
 public:
  explicit ReadExperiment(google::cloud::internal::DefaultPRNG generator)
      : BasicExperiment<Traits>(generator, "Read") {}

 protected:
  std::vector<RowCpuSample> ViaStub(
      Config const& config, int thread_count, int channel_count,
      spanner::Database const& database,
      std::shared_ptr<spanner_internal::SpannerStub> stub) override {
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
      this->impl_.LogError(std::move(os).str());
      return {};
    }

    std::vector<RowCpuSample> samples;
    // We expect about 50 reads per second per thread. Use that to estimate
    // the size of the vector.
    samples.reserve(
        static_cast<std::size_t>(config.iteration_duration.count() * 50));
    std::vector<std::string> const columns{"Key",   "Data0", "Data1", "Data2",
                                           "Data3", "Data4", "Data5", "Data6",
                                           "Data7", "Data8", "Data9"};
    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.iteration_duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto key = this->impl_.RandomKeySet(config);

      Timer timer;
      timer.Start();

      google::spanner::v1::ReadRequest request{};
      request.set_session(*session);
      request.mutable_transaction()
          ->mutable_single_use()
          ->mutable_read_only()
          ->Clear();
      request.set_table(this->table_name_);
      for (auto const& name : columns) {
        request.add_columns(name);
      }
      *request.mutable_key_set() = spanner_internal::ToProto(key);

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
          channel_count, thread_count, true, row_count, timer.elapsed_time(),
          timer.cpu_time(), google::cloud::MakeStatusFromRpcError(final)});
    }
    return samples;
  }

  std::vector<RowCpuSample> ViaClient(Config const& config, int thread_count,
                                      int channel_count,
                                      spanner::Client client) override {
    std::vector<std::string> const column_names{
        "Key",   "Data0", "Data1", "Data2", "Data3", "Data4",
        "Data5", "Data6", "Data7", "Data8", "Data9"};

    using T = typename Traits::native_type;
    using RowType = std::tuple<std::int64_t, T, T, T, T, T, T, T, T, T, T>;
    std::vector<RowCpuSample> samples;
    // We expect about 50 reads per second per thread, so allocate enough
    // memory to start.
    samples.reserve(
        static_cast<std::size_t>(config.iteration_duration.count() * 50));
    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.iteration_duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto key = this->impl_.RandomKeySet(config);

      Timer timer;
      timer.Start();
      auto rows = client.Read(this->table_name_, key, column_names);
      int row_count = 0;
      Status status;
      for (auto& row : spanner::StreamOf<RowType>(rows)) {
        if (!row) {
          status = std::move(row).status();
          break;
        }
        ++row_count;
      }
      timer.Stop();
      samples.push_back(RowCpuSample{channel_count, thread_count, false,
                                     row_count, timer.elapsed_time(),
                                     timer.cpu_time(), std::move(status)});
    }
    return samples;
  }
};

/**
 * Run an experiment to measure the CPU overhead of the client over raw gRPC.
 *
 * This experiments creates and populates a table with `config.table_size` rows,
 * each row containing an integer key and 10 columns of the types defined by
 * `Traits`. Then the experiment performs `config.samples` iterations of:
 *   - Randomly pick if it will do the work using the client library or raw
 *     gRPC
 *   - Then for `config.iteration_duration` seconds SELECT random ranges of
 *     `config.query_size` rows
 *   - Measure the CPU time required by the previous step
 */
template <typename Traits>
class SelectExperiment : public BasicExperiment<Traits> {
 public:
  explicit SelectExperiment(google::cloud::internal::DefaultPRNG generator)
      : BasicExperiment<Traits>(generator, "Select") {}

 protected:
  std::vector<RowCpuSample> ViaStub(
      Config const& config, int thread_count, int channel_count,
      spanner::Database const& database,
      std::shared_ptr<spanner_internal::SpannerStub> stub) override {
    auto session = [&]() -> google::cloud::StatusOr<std::string> {
      Status last_status;
      for (int i = 0; i != ExperimentImpl<Traits>::kColumnCount; ++i) {
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
      this->impl_.LogError(std::move(os).str());
      return {};
    }

    std::vector<RowCpuSample> samples;
    // We expect about 50 reads per second per thread. Use that to estimate
    // the size of the vector.
    samples.reserve(
        static_cast<std::size_t>(config.iteration_duration.count() * 50));
    auto const statement = CreateStatement();
    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.iteration_duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto key = this->impl_.RandomKeySetBegin(config);

      Timer timer;
      timer.Start();

      google::spanner::v1::ExecuteSqlRequest request{};
      request.set_session(*session);
      request.mutable_transaction()
          ->mutable_single_use()
          ->mutable_read_only()
          ->Clear();
      request.set_sql(statement);
      auto begin_type_value = spanner_internal::ToProto(spanner::Value(key));
      (*request.mutable_param_types())["begin"] =
          std::move(begin_type_value.first);
      (*request.mutable_params()->mutable_fields())["begin"] =
          std::move(begin_type_value.second);
      auto end_type_value =
          spanner_internal::ToProto(spanner::Value(key + config.query_size));
      (*request.mutable_param_types())["end"] = std::move(end_type_value.first);
      (*request.mutable_params()->mutable_fields())["end"] =
          std::move(end_type_value.second);

      int row_count = 0;
      google::spanner::v1::PartialResultSet result;
      std::vector<google::protobuf::Value> row;
      grpc::ClientContext context;
      auto stream = stub->ExecuteStreamingSql(context, request);
      for (bool success = stream->Read(&result); success;
           success = stream->Read(&result)) {
        if (result.chunked_value()) {
          // We do not handle chunked values in the benchmark.
          continue;
        }
        row.resize(ExperimentImpl<Traits>::kColumnCount);
        std::size_t index = 0;
        for (auto& value : *result.mutable_values()) {
          row[index] = std::move(value);
          if (++index == ExperimentImpl<Traits>::kColumnCount) {
            ++row_count;
            index = 0;
          }
        }
      }
      auto final = stream->Finish();
      timer.Stop();
      samples.push_back(RowCpuSample{
          channel_count, thread_count, true, row_count, timer.elapsed_time(),
          timer.cpu_time(), google::cloud::MakeStatusFromRpcError(final)});
    }
    return samples;
  }

  std::vector<RowCpuSample> ViaClient(Config const& config, int thread_count,
                                      int channel_count,
                                      spanner::Client client) override {
    auto const statement = CreateStatement();

    using T = typename Traits::native_type;
    using RowType = std::tuple<T, T, T, T, T, T, T, T, T, T>;
    std::vector<RowCpuSample> samples;
    // We expect about 50 reads per second per thread, so allocate enough
    // memory to start.
    samples.reserve(
        static_cast<std::size_t>(config.iteration_duration.count() * 50));
    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.iteration_duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto key = this->impl_.RandomKeySetBegin(config);

      Timer timer;
      timer.Start();
      auto rows = client.ExecuteQuery(spanner::SqlStatement(
          statement, {{"begin", spanner::Value(key)},
                      {"end", spanner::Value(key + config.query_size)}}));
      int row_count = 0;
      Status status;
      for (auto& row : spanner::StreamOf<RowType>(rows)) {
        if (!row) {
          status = std::move(row).status();
          break;
        }
        ++row_count;
      }
      timer.Stop();
      samples.push_back(RowCpuSample{channel_count, thread_count, false,
                                     row_count, timer.elapsed_time(),
                                     timer.cpu_time(), std::move(status)});
    }
    return samples;
  }

  std::string CreateStatement() const {
    std::string sql = "SELECT";
    char const* sep = " ";
    for (int i = 0; i != ExperimentImpl<Traits>::kColumnCount; ++i) {
      sql += sep;
      sql += "Data" + std::to_string(i);
      sep = ", ";
    }
    sql += " FROM ";
    sql += this->table_name_;
    sql += " WHERE Key >= @begin AND Key < @end";
    return sql;
  }
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
class UpdateExperiment : public BasicExperiment<Traits> {
 public:
  explicit UpdateExperiment(google::cloud::internal::DefaultPRNG generator)
      : BasicExperiment<Traits>(generator, "Update") {}

 protected:
  std::vector<RowCpuSample> ViaStub(
      Config const& config, int thread_count, int channel_count,
      spanner::Database const& database,
      std::shared_ptr<spanner_internal::SpannerStub> stub) override {
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
      this->impl_.LogError(std::move(os).str());
      return {};
    }

    std::vector<RowCpuSample> samples;
    // We expect about 50 reads per second per thread. Use that to estimate
    // the size of the vector.
    samples.reserve(
        static_cast<std::size_t>(config.iteration_duration.count() * 50));
    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.iteration_duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto const key = this->impl_.RandomKey(config);

      using T = typename Traits::native_type;
      std::vector<T> const values{
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
      };

      Timer timer;
      timer.Start();

      google::spanner::v1::ExecuteSqlRequest request{};
      request.set_session(*session);
      request.mutable_transaction()
          ->mutable_begin()
          ->mutable_read_write()
          ->Clear();
      request.set_sql(statement);
      auto key_type_value = spanner_internal::ToProto(spanner::Value(key));
      (*request.mutable_param_types())["key"] = std::move(key_type_value.first);
      (*request.mutable_params()->mutable_fields())["key"] =
          std::move(key_type_value.second);

      for (int i = 0; i != 10; ++i) {
        auto tv = spanner_internal::ToProto(spanner::Value(values[i]));
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
      samples.push_back(RowCpuSample{channel_count, thread_count, true,
                                     row_count, timer.elapsed_time(),
                                     timer.cpu_time(), status});
    }
    return samples;
  }

  std::vector<RowCpuSample> ViaClient(Config const& config, int thread_count,
                                      int channel_count,
                                      spanner::Client client) override {
    std::string const statement = CreateStatement();

    std::vector<RowCpuSample> samples;
    // We expect about 50 reads per second per thread, so allocate enough
    // memory to start.
    samples.reserve(
        static_cast<std::size_t>(config.iteration_duration.count() * 50));
    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.iteration_duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto const key = this->impl_.RandomKey(config);

      using T = typename Traits::native_type;
      std::vector<T> const values{
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
      };

      Timer timer;
      timer.Start();
      std::unordered_map<std::string, spanner::Value> const params{
          {"key", spanner::Value(key)},      {"v0", spanner::Value(values[0])},
          {"v1", spanner::Value(values[1])}, {"v2", spanner::Value(values[2])},
          {"v3", spanner::Value(values[3])}, {"v4", spanner::Value(values[4])},
          {"v5", spanner::Value(values[5])}, {"v6", spanner::Value(values[6])},
          {"v7", spanner::Value(values[7])}, {"v8", spanner::Value(values[8])},
          {"v9", spanner::Value(values[9])},
      };

      int row_count = 0;
      auto commit_result =
          client.Commit([&](spanner::Transaction const& txn)
                            -> google::cloud::StatusOr<spanner::Mutations> {
            auto result = client.ExecuteDml(
                txn, spanner::SqlStatement(statement, params));
            if (!result) return std::move(result).status();
            row_count = static_cast<int>(result->RowsModified());
            return spanner::Mutations{};
          });
      timer.Stop();
      samples.push_back(RowCpuSample{
          channel_count, thread_count, false, row_count, timer.elapsed_time(),
          timer.cpu_time(), std::move(commit_result).status()});
    }
    return samples;
  }

  std::string CreateStatement() const {
    std::string sql = "UPDATE " + this->table_name_;
    char const* sep = " SET ";
    for (int i = 0; i != 10; ++i) {
      sql += sep;
      sql += " Data" + std::to_string(i) + " = @v" + std::to_string(i);
      sep = ", ";
    }
    sql += " WHERE Key = @key";
    return sql;
  }
};

/**
 * Run an experiment to measure the CPU overhead of the client over raw gRPC.
 *
 * These experiments create an empty table. The table schema has an integer key
 * and 10 columns of the types defined by `Traits`. Then the experiment performs
 * M iterations of:
 *   - Randomly select if it will insert using the client library or raw gRPC.
 *   - Then for N seconds insert random rows
 *   - Measure the CPU time required by the previous step
 *
 * The values of K, M, N are configurable.
 */
template <typename Traits>
class MutationExperiment : public BasicExperiment<Traits> {
 public:
  explicit MutationExperiment(google::cloud::internal::DefaultPRNG generator)
      : BasicExperiment<Traits>(generator, "Mutation") {}

 protected:
  std::vector<RowCpuSample> ViaStub(
      Config const& config, int thread_count, int channel_count,
      spanner::Database const& database,
      std::shared_ptr<spanner_internal::SpannerStub> stub) override {
    std::vector<std::string> const column_names{
        "Key",   "Data0", "Data1", "Data2", "Data3", "Data4",
        "Data5", "Data6", "Data7", "Data8", "Data9"};

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
      this->impl_.LogError(std::move(os).str());
      return {};
    }

    std::vector<RowCpuSample> samples;
    // We expect about 50 reads per second per thread. Use that to estimate
    // the size of the vector.
    samples.reserve(
        static_cast<std::size_t>(config.iteration_duration.count() * 50));
    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.iteration_duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      std::int64_t key = 0;
      {
        std::lock_guard<std::mutex> lk(mu_);
        if (random_keys_.empty()) {
          return samples;
        }
        key = random_keys_.back();
        random_keys_.pop_back();
      }

      using T = typename Traits::native_type;
      std::vector<T> const values{
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
      };

      Timer timer;
      timer.Start();

      grpc::ClientContext context;
      google::spanner::v1::CommitRequest commit_request;
      commit_request.set_session(*session);
      commit_request.mutable_single_use_transaction()
          ->mutable_read_write()
          ->Clear();
      auto& mutation =
          *commit_request.add_mutations()->mutable_insert_or_update();
      mutation.set_table(this->table_name_);
      for (auto const& c : column_names) {
        mutation.add_columns(c);
      }
      auto& row = *mutation.add_values();
      row.add_values()->set_string_value(std::to_string(key));
      for (auto v : values) {
        *row.add_values() =
            spanner_internal::ToProto(spanner::Value(std::move(v))).second;
      }
      auto response = stub->Commit(context, commit_request);

      timer.Stop();
      samples.push_back(RowCpuSample{
          channel_count, thread_count, true, commit_request.mutations_size(),
          timer.elapsed_time(), timer.cpu_time(), response.status()});
    }
    return samples;
  }

  std::vector<RowCpuSample> ViaClient(Config const& config, int thread_count,
                                      int channel_count,
                                      spanner::Client client) override {
    std::vector<std::string> const column_names{
        "Key",   "Data0", "Data1", "Data2", "Data3", "Data4",
        "Data5", "Data6", "Data7", "Data8", "Data9"};

    std::vector<RowCpuSample> samples;
    // We expect about 50 reads per second per thread, so allocate enough
    // memory to start.
    samples.reserve(
        static_cast<std::size_t>(config.iteration_duration.count() * 50));
    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.iteration_duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      std::int64_t key = 0;
      {
        std::lock_guard<std::mutex> lk(mu_);
        if (random_keys_.empty()) {
          return samples;
        }
        key = random_keys_.back();
        random_keys_.pop_back();
      }

      using T = typename Traits::native_type;
      std::vector<T> const values{
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
          this->impl_.GenerateRandomValue(), this->impl_.GenerateRandomValue(),
      };

      Timer timer;
      timer.Start();

      int row_count = 0;
      auto commit_result =
          client.Commit(spanner::Mutations{spanner::MakeInsertOrUpdateMutation(
              this->table_name_, column_names, key, values[0], values[1],
              values[2], values[3], values[4], values[5], values[6], values[7],
              values[8], values[9])});
      timer.Stop();
      samples.push_back(RowCpuSample{
          channel_count, thread_count, false, row_count, timer.elapsed_time(),
          timer.cpu_time(), std::move(commit_result).status()});
    }
    return samples;
  }

  std::mutex mu_;
  std::vector<int64_t> random_keys_;
};

using ExperimentFactory = std::function<std::unique_ptr<Experiment>(
    google::cloud::internal::DefaultPRNG)>;

std::map<std::string, ExperimentFactory> AvailableExperiments();

class RunAllExperiment : public Experiment {
 public:
  explicit RunAllExperiment(google::cloud::internal::DefaultPRNG generator)
      : generator_(generator) {}

  std::string AdditionalDdlStatement() override { return {}; }
  Status SetUp(Config const&, spanner::Database const&) override {
    setup_called_ = true;
    return {};
  }

  Status Run(Config const& cfg, spanner::Database const& database) override {
    // Smoke test all the experiments by running a very small version of each.

    Status last_error;
    for (auto& kv : AvailableExperiments()) {
      // Do not recurse, skip this experiment.
      if (kv.first == "run-all") continue;
      // TODO(#5024): Remove this check when the emulator supports NUMERIC.
      if (google::cloud::internal::GetEnv("SPANNER_EMULATOR_HOST")
              .has_value()) {
        auto pos = kv.first.rfind("-numeric");
        if (pos != std::string::npos) {
          continue;
        }
      }

      Config config = cfg;
      config.experiment = kv.first;
      config.samples = 1;
      config.iteration_duration = std::chrono::seconds(1);
      config.minimum_threads = 1;
      config.maximum_threads = 1;
      config.minimum_channels = 1;
      config.maximum_channels = 1;
      config.table_size = 10;
      config.query_size = 1;

      auto experiment = kv.second(generator_);

      std::cout << "# Smoke test for experiment\n";
      std::cout << config << "\n" << std::flush;
      if (setup_called_) {
        // Only call SetUp() on each experiment if our own SetUp() was called.
        auto status = experiment->SetUp(config, database);
        if (!status.ok()) {
          std::cout << "# ERROR in SetUp: " << status << "\n";
          last_error = status;
          continue;
        }
      }
      config.use_only_clients = true;
      config.use_only_stubs = false;
      experiment->Run(config, database);
      config.use_only_clients = false;
      config.use_only_stubs = true;
      experiment->Run(config, database);
    }

    return last_error;
  }

 private:
  bool setup_called_ = false;
  std::mutex mu_;
  google::cloud::internal::DefaultPRNG generator_;
};

template <typename Trait>
ExperimentFactory MakeReadFactory() {
  using G = ::google::cloud::internal::DefaultPRNG;
  return [](G g) { return absl::make_unique<ReadExperiment<Trait>>(g); };
}

template <typename Trait>
ExperimentFactory MakeSelectFactory() {
  using G = ::google::cloud::internal::DefaultPRNG;
  return [](G g) { return absl::make_unique<SelectExperiment<Trait>>(g); };
}

template <typename Trait>
ExperimentFactory MakeUpdateFactory() {
  using G = ::google::cloud::internal::DefaultPRNG;
  return [](G g) { return absl::make_unique<UpdateExperiment<Trait>>(g); };
}

template <typename Trait>
ExperimentFactory MakeMutationFactory() {
  using G = ::google::cloud::internal::DefaultPRNG;
  return [](G g) { return absl::make_unique<MutationExperiment<Trait>>(g); };
}

std::map<std::string, ExperimentFactory> AvailableExperiments() {
  auto make_run_all = [](google::cloud::internal::DefaultPRNG g) {
    return absl::make_unique<RunAllExperiment>(g);
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
      {"read-numeric", MakeReadFactory<NumericTraits>()},
      {"select-bool", MakeSelectFactory<BoolTraits>()},
      {"select-bytes", MakeSelectFactory<BytesTraits>()},
      {"select-date", MakeSelectFactory<DateTraits>()},
      {"select-float64", MakeSelectFactory<Float64Traits>()},
      {"select-int64", MakeSelectFactory<Int64Traits>()},
      {"select-string", MakeSelectFactory<StringTraits>()},
      {"select-timestamp", MakeSelectFactory<TimestampTraits>()},
      {"select-numeric", MakeSelectFactory<NumericTraits>()},
      {"update-bool", MakeUpdateFactory<BoolTraits>()},
      {"update-bytes", MakeUpdateFactory<BytesTraits>()},
      {"update-date", MakeUpdateFactory<DateTraits>()},
      {"update-float64", MakeUpdateFactory<Float64Traits>()},
      {"update-int64", MakeUpdateFactory<Int64Traits>()},
      {"update-string", MakeUpdateFactory<StringTraits>()},
      {"update-timestamp", MakeUpdateFactory<TimestampTraits>()},
      {"update-numeric", MakeUpdateFactory<NumericTraits>()},
      {"mutation-bool", MakeMutationFactory<BoolTraits>()},
      {"mutation-bytes", MakeMutationFactory<BytesTraits>()},
      {"mutation-date", MakeMutationFactory<DateTraits>()},
      {"mutation-float64", MakeMutationFactory<Float64Traits>()},
      {"mutation-int64", MakeMutationFactory<Int64Traits>()},
      {"mutation-string", MakeMutationFactory<StringTraits>()},
      {"mutation-timestamp", MakeMutationFactory<TimestampTraits>()},
      {"mutation-numeric", MakeMutationFactory<NumericTraits>()},
  };
}

}  // namespace

int main(int argc, char* argv[]) {
  // Set any "sticky" I/O format flags before we fork threads.
  std::cout.setf(std::ios::boolalpha);

  Config config;
  {
    std::vector<std::string> args{argv, argv + argc};
    auto c = google::cloud::spanner_benchmarks::ParseArgs(args);
    if (!c) {
      std::cerr << "Error parsing command-line arguments: " << c.status()
                << "\n";
      return 1;
    }
    config = *std::move(c);
  }

  if (!SupportPerThreadUsage() && config.maximum_threads > 1) {
    std::cerr << "Your platform does not support per-thread getrusage() data."
              << " The benchmark cannot run with more than one thread, and you"
              << " set maximum threads to " << config.maximum_threads << "\n";
    return 1;
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

  // If the user specified a database name on the command line, re-use it to
  // reduce setup time when running the benchmark repeatedly. It's assumed that
  // other flags related to database creation have not been changed across runs.
  bool user_specified_database = !config.database_id.empty();
  if (!user_specified_database) {
    config.database_id =
        google::cloud::spanner_testing::RandomDatabaseName(generator);
  }
  google::cloud::spanner::Database database(
      config.project_id, config.instance_id, config.database_id);

  // Once the configuration is fully initialized and the database name set,
  // print everything out.
  std::cout << config << std::flush;

  google::cloud::spanner::DatabaseAdminClient admin_client;
  std::vector<std::string> additional_statements = [&available, generator] {
    std::vector<std::string> statements;
    for (auto const& kv : available) {
      // TODO(#5024): Remove this check when the emulator supports NUMERIC.
      if (google::cloud::internal::GetEnv("SPANNER_EMULATOR_HOST")
              .has_value()) {
        auto pos = kv.first.rfind("-numeric");
        if (pos != std::string::npos) {
          continue;
        }
      }
      auto experiment = kv.second(generator);
      auto s = experiment->AdditionalDdlStatement();
      if (s.empty()) continue;
      statements.push_back(std::move(s));
    }
    return statements;
  }();

  std::cout << "# Waiting for database creation to complete " << std::flush;
  google::cloud::StatusOr<google::spanner::admin::database::v1::Database> db;
  int constexpr kMaxCreateDatabaseRetries = 3;
  for (int retry = 0; retry <= kMaxCreateDatabaseRetries; ++retry) {
    auto create_future =
        admin_client.CreateDatabase(database, additional_statements);
    for (;;) {
      auto status = create_future.wait_for(std::chrono::seconds(1));
      if (status == std::future_status::ready) break;
      std::cout << '.' << std::flush;
    }
    db = create_future.get();
    if (db) break;
    if (db.status().code() != google::cloud::StatusCode::kUnavailable) break;
    std::this_thread::sleep_for(retry * std::chrono::seconds(3));
  }
  std::cout << " DONE\n";

  bool database_created = true;
  if (!db) {
    if (user_specified_database &&
        db.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "# Re-using existing database\n";
      database_created = false;
    } else {
      std::cerr << "Error creating database: " << db.status() << "\n";
      return 1;
    }
  }

  std::cout << "ChannelCount,ThreadCount,UsingStub"
            << ",RowCount,ElapsedTime,CpuTime,StatusCode\n"
            << std::flush;

  int exit_status = EXIT_SUCCESS;

  auto experiment = e->second(generator);
  Status setup_status;
  if (database_created) {
    setup_status = experiment->SetUp(config, database);
    if (!setup_status.ok()) {
      std::cout << "# Skipping experiment, SetUp() failed: " << setup_status
                << "\n";
      exit_status = EXIT_FAILURE;
    }
  }
  if (setup_status.ok()) {
    auto run_status = experiment->Run(config, database);
    if (!run_status.ok()) exit_status = EXIT_FAILURE;
  }

  if (!user_specified_database) {
    auto drop = admin_client.DropDatabase(database);
    if (!drop.ok()) {
      std::cerr << "# Error dropping database: " << drop << "\n";
    }
  }
  std::cout << "# Experiment finished, "
            << (user_specified_database ? "user-specified database kept\n"
                                        : "database dropped\n");
  return exit_status;
}
