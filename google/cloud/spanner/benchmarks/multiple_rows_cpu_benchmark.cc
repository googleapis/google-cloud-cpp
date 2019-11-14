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

using SampleSink = std::function<void(std::vector<RowCpuSample>)>;

class Experiment {
 public:
  virtual ~Experiment() = default;

  virtual Status SetUp(Config const& config, cs::Database const& database) = 0;
  virtual Status TearDown(Config const& config,
                          cs::Database const& database) = 0;
  virtual Status Run(google::cloud::internal::DefaultPRNG generator,
                     Config const& config, cs::Database const& database,
                     SampleSink const& sink) = 0;
};

std::map<std::string, std::shared_ptr<Experiment>> AvailableExperiments();

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

  // Once the configuration is validated, print it out.
  std::cout << config << std::flush;

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

  std::mutex cout_mu;
  auto cout_sink =
      [&cout_mu](std::vector<RowCpuSample> const& samples) mutable {
        std::unique_lock<std::mutex> lk(cout_mu);
        for (auto const& s : samples) {
          std::cout << std::boolalpha << s.client_count << ',' << s.thread_count
                    << ',' << s.using_stub << ',' << s.row_count << ','
                    << s.elapsed.count() << ',' << s.cpu_time.count() << ','
                    << s.status.code() << '\n'
                    << std::flush;
        }
      };

  auto experiment = e->second;
  experiment->SetUp(config, database);
  experiment->Run(generator, config, database, cout_sink);
  experiment->TearDown(config, database);

  auto drop = admin_client.DropDatabase(database);
  if (!drop.ok()) {
    std::cerr << "# Error dropping database: " << drop << "\n";
  }
  std::cout << "# Experiment finished, database dropped\n";
  return 0;
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

/**
 * Run an experiment to measure the CPU overhead of the client over raw gRPC.
 *
 * This experiments creates and populates a table with K rows, each row
 * containing an (integer) key and a value with a 1KiB string. Then the
 * experiment performs M iterations of:
 *   - Randomly select if it will read using the client library or raw gRPC.
 *   - Then for N seconds read a random row
 *   - Measure the CPU time required to read the row
 *
 * The values of K, M, N are configurable. The results are reported to a
 * `SampleSink` object, typically a this is a (thread-safe) function that prints
 * to `std::cout`. We use separate scripts to analyze the results.
 */
class ReadExperiment : public Experiment {
 public:
  ReadExperiment() = default;

  Status SetUp(Config const& config, cs::Database const& database) override {
    cs::DatabaseAdminClient admin_client;
    auto created =
        admin_client.UpdateDatabase(database, {R"sql(CREATE TABLE KeyValue (
                                Key   INT64 NOT NULL,
                                Data  STRING(1024),
                             ) PRIMARY KEY (Key))sql"});
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

    // We need to populate some data or all the requests to read will fail.
    cs::Client client(cs::MakeConnection(database));
    std::cout << "# Populating database " << std::flush;
    int const task_count = 16;
    std::vector<std::future<void>> tasks(task_count);
    int task_id = 0;
    for (auto& t : tasks) {
      t = std::async(
          std::launch::async,
          [this, &config, &client](int tc, int ti) {
            SetUpTask(config, client, tc, ti);
          },
          task_count, task_id++);
    }
    for (auto& t : tasks) {
      t.get();
    }
    std::cout << " DONE\n";
    return {};
  }

  Status TearDown(Config const&, cs::Database const&) override { return {}; }

  Status Run(google::cloud::internal::DefaultPRNG generator,
             Config const& config, cs::Database const& database,
             SampleSink const& sink) override {
    generator_ = generator;
    // Create enough clients and stubs for the worst case
    std::vector<cs::Client> clients;
    std::vector<std::shared_ptr<cs::internal::SpannerStub>> stubs;
    std::cout << "# Creating clients " << std::flush;
    for (int i = 0; i != config.maximum_clients; ++i) {
      auto options = cs::ConnectionOptions().set_channel_pool_domain(
          "task:" + std::to_string(i));
      clients.emplace_back(cs::Client(cs::MakeConnection(database, options)));
      stubs.emplace_back(cs::internal::CreateDefaultSpannerStub(options));
      std::cout << '.' << std::flush;
    }
    std::cout << " DONE\n";

    std::uniform_int_distribution<int> use_stubs_gen(0, 1);
    std::uniform_int_distribution<int> thread_count_gen(config.minimum_threads,
                                                        config.maximum_threads);

    // Capture some overall getrusage() statistics as comments.
    SimpleTimer overall;
    overall.Start();
    for (int i = 0; i != config.samples; ++i) {
      auto const use_stubs = use_stubs_gen(generator_) == 1;
      auto const thread_count = thread_count_gen(generator_);
      // TODO(#1000) - avoid deadlocks with more than 100 threads per client
      auto const min_clients = (std::max<std::size_t>)(thread_count / 100 + 1,
                                                       config.minimum_clients);
      auto const max_clients = clients.size();
      auto const client_count = [min_clients, max_clients, this] {
        if (min_clients <= max_clients) {
          return min_clients;
        }
        return std::uniform_int_distribution<std::size_t>(
            min_clients, max_clients - 1)(generator_);
      }();
      if (use_stubs) {
        std::vector<std::shared_ptr<cs::internal::SpannerStub>> iteration_stubs(
            stubs.begin(), stubs.begin() + client_count);
        RunIterationViaStubs(config, iteration_stubs, thread_count, sink);
        continue;
      }
      std::vector<cs::Client> iteration_clients(clients.begin(),
                                                clients.begin() + client_count);
      RunIterationViaClients(config, iteration_clients, thread_count, sink);
    }
    overall.Stop();
    std::cout << overall.annotations();
    return {};
  }

 private:
  void RunIterationViaStubs(
      Config const& config,
      std::vector<std::shared_ptr<cs::internal::SpannerStub>> const& stubs,
      int thread_count, SampleSink const& sink) {
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
      sink(t.get());
    }
  }

  std::vector<RowCpuSample> ReadRowsViaStub(
      Config const& config, int thread_count, int client_count,
      cs::Database const& database,
      std::shared_ptr<cs::internal::SpannerStub> stub) {
    auto session = [&]() -> google::cloud::StatusOr<std::string> {
      grpc::ClientContext context;
      google::spanner::v1::CreateSessionRequest request{};
      request.set_database(database.FullName());
      auto response = stub->CreateSession(context, request);
      if (!response) return std::move(response).status();
      return response->name();
    }();

    std::vector<RowCpuSample> samples;
    // We expect about 50 reads per second per thread. Use that to estimate the
    // size of the vector.
    samples.reserve(config.iteration_duration.count() * 50);
    std::vector<google::cloud::Status> errors;
    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.iteration_duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto key = RandomKeySet(config);

      SimpleTimer timer;
      timer.Start();

      grpc::ClientContext context;
      google::spanner::v1::ReadRequest request{};
      request.set_session(*session);
      request.set_table("KeyValue");
      request.add_columns("Key");
      request.add_columns("Data");
      *request.mutable_key_set() = cs::internal::ToProto(key);
      auto stream = stub->StreamingRead(context, request);
      int row_count = 0;
      google::spanner::v1::PartialResultSet result;
      for (bool success = stream->Read(&result); success;
           success = stream->Read(&result)) {
        if (!result.chunked_value()) {
          row_count += result.values_size() / 2;
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
                              int thread_count, SampleSink const& sink) {
    std::vector<std::future<std::vector<RowCpuSample>>> tasks(thread_count);
    int task_id = 0;
    for (auto& t : tasks) {
      auto client = clients[task_id++ % clients.size()];
      t = std::async(std::launch::async, &ReadExperiment::ReadRowsViaClients,
                     this, config, thread_count,
                     static_cast<int>(clients.size()), client);
    }
    for (auto& t : tasks) {
      sink(t.get());
    }
  }

  std::vector<RowCpuSample> ReadRowsViaClients(Config const& config,
                                               int thread_count,
                                               int client_count,
                                               cs::Client client) {
    std::vector<RowCpuSample> samples;
    // We expect about 50 reads per second per thread, so allocate enough
    // memory to start.
    samples.reserve(config.iteration_duration.count() * 50);
    std::vector<google::cloud::Status> errors;
    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.iteration_duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto key = RandomKeySet(config);

      SimpleTimer timer;
      timer.Start();
      auto rows = client.Read("KeyValue", key, {"Key", "Data"});
      int row_count = 0;
      Status status;
      for (auto& row :
           cs::StreamOf<std::tuple<std::int64_t, std::string>>(rows)) {
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

  cs::KeySet RandomKeySet(Config const& config) {
    std::lock_guard<std::mutex> lk(mu_);
    auto begin = std::uniform_int_distribution<std::int64_t>(
        0, config.table_size - config.query_size)(generator_);
    auto end = begin + config.query_size - 1;
    return cs::KeySet().AddRange(cs::MakeKeyBoundClosed(cs::Value(begin)),
                                 cs::MakeKeyBoundClosed(cs::Value(end)));
  }

  void SetUpTask(Config const& config, cs::Client client, int task_count,
                 int task_id) {
    std::string value = [this] {
      std::lock_guard<std::mutex> lk(mu_);
      return google::cloud::internal::Sample(
          generator_, 1024, "#@$%^&*()-=+_0123456789[]{}|;:,./<>?");
    }();

    auto mutation =
        cs::InsertOrUpdateMutationBuilder("KeyValue", {"Key", "Data"});
    int current_mutations = 0;

    auto maybe_flush = [&mutation, &current_mutations, &client,
                        this](bool force) {
      if (current_mutations == 0) {
        return;
      }
      if (!force && current_mutations < 1000) {
        return;
      }
      auto m = std::move(mutation).Build();
      auto result = client.Commit(
          [&m](cs::Transaction const&) { return cs::Mutations{m}; });
      if (!result) {
        std::lock_guard<std::mutex> lk(mu_);
        std::cerr << "# Error in Commit() " << result.status() << "\n";
      }
      mutation = cs::InsertOrUpdateMutationBuilder("KeyValue", {"Key", "Data"});
      current_mutations = 0;
    };
    auto force_flush = [&maybe_flush] { maybe_flush(true); };
    auto flush_as_needed = [&maybe_flush] { maybe_flush(false); };

    auto const report_period =
        (std::max)(static_cast<std::int64_t>(2), config.table_size / 50);
    for (std::int64_t key = 0; key != config.table_size; ++key) {
      // Each thread does a fraction of the key space.
      if (key % task_count != task_id) continue;
      // Have one of the threads report progress about 50 times.
      if (task_id == 0 && key % report_period == 0) {
        std::cout << '.' << std::flush;
      }
      mutation.EmplaceRow(key, value);
      current_mutations++;
      flush_as_needed();
    }
    force_flush();
  }

  std::mutex mu_;
  google::cloud::internal::DefaultPRNG generator_;
};

class RunAllExperiment : public Experiment {
 public:
  Status SetUp(Config const&, cs::Database const&) override { return {}; }
  Status TearDown(Config const&, cs::Database const&) override { return {}; }

  Status Run(google::cloud::internal::DefaultPRNG generator, Config const& cfg,
             cs::Database const& database, SampleSink const&) override {
    // Smoke test all the experiments by running a very small version of each.
    for (auto& kv : AvailableExperiments()) {
      // Do not recurse, skip this experiment.
      if (kv.first == "run-all") continue;
      Config config = cfg;
      config.experiment = kv.first;
      config.samples = 2;
      config.iteration_duration = std::chrono::seconds(1);
      config.minimum_threads = 1;
      config.maximum_threads = 1;
      config.minimum_clients = 1;
      config.maximum_clients = 1;
      config.table_size = 10;
      config.query_size = 1;
      std::cout << "# Smoke test for experiment: " << kv.first << "\n";
      auto discard = [](std::vector<RowCpuSample> const&) {};
      kv.second->SetUp(config, database);
      kv.second->Run(generator, config, database, discard);
      kv.second->TearDown(config, database);
    }
    return {};
  }
};

std::map<std::string, std::shared_ptr<Experiment>> AvailableExperiments() {
  return {
      {"run-all", std::make_shared<RunAllExperiment>()},
      {"read", std::make_shared<ReadExperiment>()},
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
