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
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include <algorithm>
#include <future>
#include <random>
#include <sstream>
#include <thread>

namespace {

namespace spanner = ::google::cloud::spanner;

struct SingleRowThroughputSample {
  int client_count;
  int thread_count;
  int event_count;
  std::chrono::microseconds elapsed;
};

using ::google::cloud::spanner_benchmarks::Config;
using SampleSink = std::function<void(std::vector<SingleRowThroughputSample>)>;
using RandomKeyGenerator = std::function<std::int64_t()>;
using ErrorSink = std::function<void(std::vector<google::cloud::Status>)>;

spanner::Client MakeClient(Config const& config, int num_channels,
                           spanner::Database const& database) {
  std::cout << "# Creating 1 client using shared connection with "
            << num_channels << " channels\n"
            << std::flush;

  auto connection = spanner::MakeConnection(
      database, spanner::ConnectionOptions().set_num_channels(num_channels),
      // This pre-creates all the Sessions we will need (one per thread).
      spanner::SessionPoolOptions().set_min_sessions(config.maximum_threads));
  return spanner::Client(std::move(connection));
}

class Experiment {
 public:
  virtual ~Experiment() = default;

  virtual void SetUp(Config const& config,
                     spanner::Database const& database) = 0;

  virtual void Run(Config const& config, spanner::Database const& database,
                   SampleSink const& sink) = 0;
};

class BasicExperiment : public Experiment {
 public:
  BasicExperiment() : generator_(std::random_device{}()) {}

  void Run(Config const& config, spanner::Database const& database,
           SampleSink const& sink) override {
    std::cout << config << std::flush;

    std::uniform_int_distribution<int> thread_count_gen(config.minimum_threads,
                                                        config.maximum_threads);

    std::uniform_int_distribution<int> channel_count_gen(
        config.minimum_channels, config.maximum_channels);
    for (int i = 0; i != config.samples; ++i) {
      auto const thread_count = thread_count_gen(generator_);
      auto const channel_count = channel_count_gen(generator_);
      RunIteration(config, MakeClient(config, channel_count, database),
                   channel_count, thread_count, sink);
    }
  }

 protected:
  void BasicSetUp(Config const& config, spanner::Database const& database) {
    std::string value = [this] {
      std::lock_guard<std::mutex> lk(mu_);
      return google::cloud::internal::Sample(
          generator_, 1024, "#@$%^&*()-=+_0123456789[]{}|;:,./<>?");
    }();
    FillTable(config, database, value);
  }

  void FillTable(Config const& config, spanner::Database const& database,
                 std::string const& value) {
    // We need to populate some data or all the requests to read will fail.
    spanner::Client client(spanner::MakeConnection(database));
    std::cout << "# Populating database " << std::flush;
    int const task_count = 16;
    std::vector<std::future<void>> tasks(task_count);
    int task_id = 0;
    for (auto& t : tasks) {
      t = std::async(
          std::launch::async,
          [this, &config, &client, &value](int tc, int ti) {
            FillTableTask(config, client, value, tc, ti);
          },
          task_count, task_id++);
    }
    for (auto& t : tasks) {
      t.get();
    }
    std::cout << " DONE\n";
  }

  void FillTableTask(Config const& config, spanner::Client client,
                     std::string const& value, int task_count, int task_id) {
    auto mutation =
        spanner::InsertOrUpdateMutationBuilder("KeyValue", {"Key", "Data"});
    int current_mutations = 0;

    auto maybe_flush = [this, &mutation, &current_mutations,
                        &client](bool force) {
      if (current_mutations == 0) {
        return;
      }
      if (!force && current_mutations < 1000) {
        return;
      }
      auto result =
          client.Commit(spanner::Mutations{std::move(mutation).Build()});
      if (!result) {
        std::lock_guard<std::mutex> lk(mu_);
        std::cerr << "# Error in Commit() " << result.status() << "\n";
      }
      mutation =
          spanner::InsertOrUpdateMutationBuilder("KeyValue", {"Key", "Data"});
      current_mutations = 0;
    };
    auto force_flush = [&maybe_flush] { maybe_flush(true); };
    auto flush_as_needed = [&maybe_flush] { maybe_flush(false); };

    auto const report_period =
        (std::max)(static_cast<std::int32_t>(2), config.table_size / 50);
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

  void RunIteration(Config const& config, spanner::Client const& client,
                    int channel_count, int thread_count,
                    SampleSink const& sink) {
    std::uniform_int_distribution<std::int64_t> random_key(0,
                                                           config.table_size);
    RandomKeyGenerator locked_random_key = [this, &random_key] {
      std::lock_guard<std::mutex> lk(mu_);
      return random_key(generator_);
    };

    std::mutex cerr_mu;
    ErrorSink error_sink =
        [&cerr_mu](std::vector<google::cloud::Status> const& errors) {
          std::lock_guard<std::mutex> lk(cerr_mu);
          for (auto const& e : errors) {
            std::cerr << "# " << e << "\n";
          }
        };

    std::vector<std::future<int>> tasks(thread_count);
    auto start = std::chrono::steady_clock::now();
    for (auto& t : tasks) {
      t = std::async(std::launch::async, [this, &config, &client,
                                          &locked_random_key, &error_sink] {
        return RunTask(config, client, locked_random_key, error_sink);
      });
    }
    int event_count = 0;
    for (auto& t : tasks) {
      event_count += t.get();
    }
    auto elapsed = std::chrono::steady_clock::now() - start;

    sink({SingleRowThroughputSample{
        channel_count, thread_count, event_count,
        std::chrono::duration_cast<std::chrono::microseconds>(elapsed)}});
  }

  virtual int RunTask(Config const& config, spanner::Client client,
                      RandomKeyGenerator const& key_generator,
                      ErrorSink const& error_sink) = 0;

 private:
  std::mutex mu_;
  google::cloud::internal::DefaultPRNG generator_;
};

class InsertOrUpdateExperiment : public BasicExperiment {
 public:
  InsertOrUpdateExperiment() = default;

  void SetUp(Config const&, spanner::Database const&) override {}

  int RunTask(Config const& config, spanner::Client client,
              RandomKeyGenerator const& key_generator,
              ErrorSink const& error_sink) override {
    int count = 0;
    std::string value(1024, 'A');
    std::vector<google::cloud::Status> errors;
    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.iteration_duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto key = key_generator();
      auto result =
          client.Commit(spanner::Mutations{spanner::MakeInsertOrUpdateMutation(
              "KeyValue", {"Key", "Data"}, key, value)});
      if (!result) {
        errors.push_back(std::move(result).status());
      }
      ++count;
    }
    error_sink(std::move(errors));
    return count;
  }
};

class ReadExperiment : public BasicExperiment {
 public:
  ReadExperiment() = default;

  void SetUp(Config const& config, spanner::Database const& database) override {
    BasicSetUp(config, database);
  }

  int RunTask(Config const& config, spanner::Client client,
              RandomKeyGenerator const& key_generator,
              ErrorSink const& error_sink) override {
    int count = 0;
    std::string value(1024, 'A');
    std::vector<google::cloud::Status> errors;
    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.iteration_duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto key = key_generator();
      auto rows = client.Read("KeyValue",
                              spanner::KeySet().AddKey(spanner::MakeKey(key)),
                              {"Key", "Data"});
      for (auto& row :
           spanner::StreamOf<std::tuple<std::int64_t, std::string>>(rows)) {
        if (!row) {
          errors.push_back(std::move(row).status());
          break;
        }
        ++count;
      }
    }
    error_sink(std::move(errors));
    return count;
  }
};

class UpdateDmlExperiment : public BasicExperiment {
 public:
  UpdateDmlExperiment() = default;

  void SetUp(Config const& config, spanner::Database const& database) override {
    BasicSetUp(config, database);
  }

  int RunTask(Config const& config, spanner::Client client,
              RandomKeyGenerator const& key_generator,
              ErrorSink const& error_sink) override {
    int count = 0;
    std::string value(1024, 'A');
    std::vector<google::cloud::Status> errors;
    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.iteration_duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto key = key_generator();
      auto result =
          client.Commit([&client, key, &value](spanner::Transaction const& txn)
                            -> google::cloud::StatusOr<spanner::Mutations> {
            auto result = client.ExecuteDml(
                txn, spanner::SqlStatement(
                         "UPDATE KeyValue SET Data = @data WHERE Key = @key",
                         {{"key", spanner::Value(key)},
                          {"data", spanner::Value(value)}}));
            if (!result) return std::move(result).status();
            return spanner::Mutations{};
          });
      if (!result) {
        errors.push_back(std::move(result).status());
      }
      ++count;
    }
    error_sink(std::move(errors));
    return count;
  }
};

class SelectExperiment : public BasicExperiment {
 public:
  SelectExperiment() = default;

  void SetUp(Config const& config, spanner::Database const& database) override {
    BasicSetUp(config, database);
  }

  int RunTask(Config const& config, spanner::Client client,
              RandomKeyGenerator const& key_generator,
              ErrorSink const& error_sink) override {
    int count = 0;
    std::string value(1024, 'A');
    std::vector<google::cloud::Status> errors;
    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.iteration_duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto key = key_generator();
      auto rows = client.ExecuteQuery(
          spanner::SqlStatement("SELECT Key, Data FROM KeyValue"
                                " WHERE Key = @key",
                                {{"key", spanner::Value(key)}}));
      for (auto& row :
           spanner::StreamOf<std::tuple<std::int64_t, std::string>>(rows)) {
        if (!row) {
          errors.push_back(std::move(row).status());
          break;
        }
        ++count;
      }
    }
    error_sink(std::move(errors));
    return count;
  }
};

std::map<std::string, std::shared_ptr<Experiment>> AvailableExperiments();

class RunAllExperiment : public Experiment {
 public:
  void SetUp(Config const&, spanner::Database const&) override {
    setup_called_ = true;
  }

  void Run(Config const& cfg, spanner::Database const& database,
           SampleSink const& sink) override {
    // Smoke test all the experiments by running a very small version of each.
    for (auto& kv : AvailableExperiments()) {
      // Do not recurse, skip this experiment.
      if (kv.first == "run-all") continue;
      Config config = cfg;
      config.table_size = 10;
      config.samples = 1;
      config.iteration_duration = std::chrono::seconds(1);
      std::cout << "# Smoke test for experiment: " << kv.first << "\n";
      if (setup_called_) {
        // Only call SetUp() on each experiment if our own SetUp() was called.
        kv.second->SetUp(config, database);
      }
      kv.second->Run(config, database, sink);
    }
  }

 private:
  bool setup_called_ = false;
};

std::map<std::string, std::shared_ptr<Experiment>> AvailableExperiments() {
  return {
      {"run-all", std::make_shared<RunAllExperiment>()},
      {"insert-or-update", std::make_shared<InsertOrUpdateExperiment>()},
      {"read", std::make_shared<ReadExperiment>()},
      {"update", std::make_shared<UpdateDmlExperiment>()},
      {"select", std::make_shared<SelectExperiment>()},
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
  auto available = AvailableExperiments();
  auto e = available.find(config.experiment);
  if (e == available.end()) {
    std::cerr << "Experiment " << config.experiment << " not found\n";
    return 1;
  }

  google::cloud::spanner::DatabaseAdminClient admin_client;

  std::cout << "# Waiting for database creation to complete " << std::flush;
  google::cloud::StatusOr<google::spanner::admin::database::v1::Database> db;
  int constexpr kMaxCreateDatabaseRetries = 3;
  for (int retry = 0; retry <= kMaxCreateDatabaseRetries; ++retry) {
    auto create_future =
        admin_client.CreateDatabase(database, {R"sql(CREATE TABLE KeyValue (
                                Key   INT64 NOT NULL,
                                Data  STRING(1024),
                             ) PRIMARY KEY (Key))sql"});
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

  std::cout << "ChannelCount,ThreadCount,EventCount,ElapsedTime\n"
            << std::flush;

  std::mutex cout_mu;
  auto cout_sink =
      [&cout_mu](
          std::vector<SingleRowThroughputSample> const& samples) mutable {
        std::unique_lock<std::mutex> lk(cout_mu);
        for (auto const& s : samples) {
          std::cout << s.client_count << ',' << s.thread_count << ','
                    << s.event_count << ',' << s.elapsed.count() << '\n'
                    << std::flush;
        }
      };

  auto experiment = e->second;
  if (database_created) {
    experiment->SetUp(config, database);
  }
  experiment->Run(config, database, cout_sink);

  if (!user_specified_database) {
    auto drop = admin_client.DropDatabase(database);
    if (!drop.ok()) {
      std::cerr << "# Error dropping database: " << drop << "\n";
    }
  }
  std::cout << "# Experiment finished, "
            << (user_specified_database ? "user-specified database kept\n"
                                        : "database dropped\n");
  return 0;
}
