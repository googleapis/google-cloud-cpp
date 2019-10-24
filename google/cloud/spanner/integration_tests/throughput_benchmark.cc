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
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include <future>
#include <random>
#include <thread>

namespace {

namespace cloud_spanner = google::cloud::spanner;

struct Config {
  std::string experiment;

  std::string project_id;
  std::string instance_id;

  std::chrono::seconds duration = std::chrono::seconds(30);

  int threads = 0;  // 0 means use the threads_per_core setting.
  int threads_per_core = 4;

  bool shared_client = true;
  std::int64_t table_size = 10 * 1000 * 1000;

  std::int64_t maximum_read_size = 10 * 1000;
  int mutations_per_request = 1000;
};

google::cloud::StatusOr<Config> ParseArgs(std::vector<std::string> args);

enum class Operation { kInsert, kSelect };

char const* OperationName(Operation op) {
  switch (op) {
    case Operation::kInsert:
      return "INSERT";
    case Operation::kSelect:
      return "SELECT";
  }
  return "UNKNOWN";
}

struct Sample {
  Operation op;
  int size;
  std::chrono::microseconds elapsed;
  bool success;
};

int TaskCount(Config const& config) {
  if (config.threads != 0) return config.threads;
  auto cores = std::thread::hardware_concurrency();
  return cores == 0 ? config.threads_per_core
                    : static_cast<int>(cores) * config.threads_per_core;
}

std::chrono::microseconds ElapsedTime(
    std::chrono::steady_clock::time_point start) {
  return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - start);
}

using SampleSink = std::function<void(std::vector<Sample>)>;

class Experiment {
 public:
  virtual ~Experiment() = default;

  virtual void SetUp(Config const& config,
                     cloud_spanner::Database const& database) = 0;
  virtual void Run(Config const& config, cloud_spanner::Client client,
                   SampleSink const& sink) = 0;
};

/// Run @p task_count copies of @p task in different threads
void RunParallel(int task_count,
                 std::function<void(int task_count, int task_id)> const& task);

/// The work of a single thread in the 'InsertSingleRow' experiment.
class InsertSingleRow : public Experiment {
 public:
  void SetUp(Config const&, cloud_spanner::Database const&) override {}
  void Run(Config const& config, cloud_spanner::Client client,
           SampleSink const& sink) override {
    // Save the samples and report them every so many entries.
    std::size_t constexpr kReportSamples = 512;
    std::vector<Sample> samples;
    samples.reserve(kReportSamples);

    // Each thread needs its own random bits generator.
    auto generator = google::cloud::internal::MakeDefaultPRNG();
    std::uniform_int_distribution<std::int64_t> random_key(0,
                                                           config.table_size);
    std::string padding = google::cloud::internal::Sample(
        generator, 512, "#@$%^&*()-=+_0123456789[]{}|;:,./<>?");
    auto make_string = [&padding](std::string const& prefix, std::int64_t key) {
      auto s = prefix + std::to_string(key) + "    ";
      s += padding.substr(prefix.size());
      return s;
    };

    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto key = random_key(generator);
      auto m = cloud_spanner::MakeInsertOrUpdateMutation(
          "Singers", {"SingerId", "FirstName", "LastName"}, key,
          make_string("fname:", key), make_string("lname:", key));
      auto start_txn = std::chrono::steady_clock::now();
      auto result = client.Commit([&m](cloud_spanner::Transaction const&) {
        return cloud_spanner::Mutations{m};
      });
      samples.push_back(
          {Operation::kInsert, 1, ElapsedTime(start_txn), result.ok()});

      if (samples.size() >= kReportSamples) {
        sink(std::move(samples));
        samples.clear();
      }
    }
    sink(std::move(samples));
  }
};

/**
 * Run an experiment inserting random numbers of rows per transaction.
 */
class InsertMultipleRows : public Experiment {
 public:
  void SetUp(Config const&, cloud_spanner::Database const&) override {}
  void Run(Config const& config, cloud_spanner::Client client,
           SampleSink const& sink) override {
    // Save the samples and report them every so many entries.
    std::size_t constexpr kReportSamples = 512;
    std::vector<Sample> samples;
    samples.reserve(kReportSamples);

    // Each thread needs its own random bits generator.
    auto generator = google::cloud::internal::MakeDefaultPRNG();
    std::uniform_int_distribution<std::int64_t> random_key(0,
                                                           config.table_size);
    std::uniform_int_distribution<int> random_row_count(
        1, config.mutations_per_request);

    std::string padding = google::cloud::internal::Sample(
        generator, 512, "#@$%^&*()-=+_0123456789[]{}|;:,./<>?");
    auto make_string = [&padding](std::string const& prefix, std::int64_t key) {
      auto s = prefix + std::to_string(key) + "    ";
      s += padding.substr(prefix.size());
      return s;
    };

    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto row_count = random_row_count(generator);
      cloud_spanner::InsertOrUpdateMutationBuilder builder(
          "Singers", {"SingerId", "FirstName", "LastName"});
      for (auto i = 0; i != row_count; ++i) {
        auto key = random_key(generator);
        builder.EmplaceRow(key, make_string("fname:", key),
                           make_string("lname:", key));
      }
      auto start_txn = std::chrono::steady_clock::now();
      auto m = std::move(builder).Build();
      auto result = client.Commit([&m](cloud_spanner::Transaction const&) {
        return cloud_spanner::Mutations{m};
      });
      samples.push_back(
          {Operation::kInsert, row_count, ElapsedTime(start_txn), result.ok()});

      if (samples.size() >= kReportSamples) {
        sink(std::move(samples));
        samples.clear();
      }
    }
    sink(std::move(samples));
  }
};

/// The work of a single thread in the 'InsertSingleRow' experiment.
class SelectSingleRow : public Experiment {
 public:
  void SetUpTask(Config const& config, cloud_spanner::Client client,
                 int task_count, int task_id) {
    auto generator = google::cloud::internal::MakeDefaultPRNG();
    std::uniform_int_distribution<std::int64_t> random_key(0,
                                                           config.table_size);
    std::string padding = google::cloud::internal::Sample(
        generator, 500, "#@$%^&*()-=+_0123456789[]{}|;:,./<>?");
    auto make_string = [&padding](std::string const& prefix, std::int64_t key) {
      auto s = prefix + std::to_string(key) + "    ";
      s += padding;
      return s;
    };

    for (std::int64_t key = 0; key != config.table_size; ++key) {
      // Have one of the threads report progress about 50 times.
      if (task_id == 0 && key % (config.table_size / 50) == 0) {
        std::cout << '.' << std::flush;
      }
      // Each thread does a fraction of the key space.
      if (key % task_count != task_id) continue;
      auto m = cloud_spanner::MakeInsertOrUpdateMutation(
          "Singers", {"SingerId", "FirstName", "LastName"}, key,
          make_string("fname:", key), make_string("lname:", key));
      (void)client.Commit([&m](cloud_spanner::Transaction const&) {
        return cloud_spanner::Mutations{m};
      });
    }
  }

  void SetUp(Config const& config,
             cloud_spanner::Database const& database) override {
    cloud_spanner::Client client(cloud_spanner::MakeConnection(database));
    std::cout << "# Populating database " << std::flush;
    RunParallel(TaskCount(config),
                [this, &config, &client](int task_count, int task_id) {
                  SetUpTask(config, client, task_count, task_id);
                });
    std::cout << " DONE\n";
  }

  void Run(Config const& config, cloud_spanner::Client client,
           SampleSink const& sink) override {
    // Save the samples and report them every so many entries.
    std::size_t constexpr kReportSamples = 512;
    std::vector<Sample> samples;
    samples.reserve(kReportSamples);

    // Each thread needs its own random bits generator.
    auto generator = google::cloud::internal::MakeDefaultPRNG();
    std::uniform_int_distribution<std::int64_t> random_key(0,
                                                           config.table_size);

    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      if (samples.size() >= kReportSamples) {
        sink(std::move(samples));
        samples.clear();
      }

      auto key = random_key(generator);
      auto start_query = std::chrono::steady_clock::now();
      cloud_spanner::SqlStatement statement(
          R"sql(
        SELECT SingerId, FirstName, LastName
          FROM Singers
         WHERE SingerId = @key
         LIMIT 1)sql",
          {{"key", cloud_spanner::Value(key)}});
      auto reader = client.ExecuteQuery(statement);
      for (auto& row : reader) {
        samples.push_back(
            {Operation::kSelect, 1, ElapsedTime(start_query), row.ok()});
        break;
      }
    }
    sink(std::move(samples));
  }
};

std::map<std::string, std::shared_ptr<Experiment>> ListExperiments() {
  return {
      {"insert-multiple-rows", std::make_shared<InsertMultipleRows>()},
      {"insert-single-row", std::make_shared<InsertSingleRow>()},
      {"select-single-row", std::make_shared<SelectSingleRow>()},
  };
}

/**
 * Briefly run each experiment.
 *
 * This is used to automatically run the code as part of other integration
 * tests, the intention is to detect crashes or problems that stop the tests
 * from running.
 */
void SmokeTest(Config const& setup, cloud_spanner::Database const& database,
               SampleSink const& sink) {
  Config config = setup;
  config.duration = std::chrono::seconds(1);
  config.threads = 1;
  config.table_size = 1000;
  // Keep the regression test logs clean by logging as little as possible.
  int error_count = 0;
  int success_count = 0;
  auto counter = [&error_count,
                  &success_count](std::vector<Sample> const& samples) {
    for (auto const& s : samples) {
      if (s.success) {
        ++success_count;
      } else {
        ++error_count;
      }
    }
  };
  auto check_counters = [&error_count,
                         &success_count](std::string const& experiment) {
    if (success_count == 0) {
      std::cerr << "Error in " << experiment
                << " expected at least one success\n";
      std::exit(1);
    }
    if (error_count / 100 > success_count) {
      std::cerr << "Error in " << experiment
                << " expected at most 1% failures (error_count=" << error_count
                << ", success_count=" << success_count << "\n";
      std::exit(1);
    }
    success_count = 0;
    error_count = 0;
  };
  cloud_spanner::Client client(cloud_spanner::MakeConnection(database));
  auto experiments = ListExperiments();
  for (auto& kv : experiments) {
    kv.second->SetUp(config, database);
    kv.second->Run(config, client, counter);
    check_counters(kv.first);
  }

  // Make sure the code to log samples does not crash.
  std::vector<Sample> test_samples{
      {Operation::kInsert, 0, std::chrono::microseconds(0), true},
      {Operation::kSelect, 0, std::chrono::microseconds(0), true},
  };
  sink(std::move(test_samples));
}

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

  cloud_spanner::Database database(
      config.project_id, config.instance_id,
      google::cloud::spanner_testing::RandomDatabaseName(generator));

  std::cout << std::boolalpha << "# Experiment: " << config.experiment
            << "\n# Project: " << config.project_id
            << "\n# Instance: " << config.instance_id
            << "\n# Database: " << database.database_id()
            << "\n# Duration: " << config.duration.count() << "s"
            << "\n# Tasks: " << TaskCount(config)
            << "\n# Shared Client: " << config.shared_client
            << "\n# Table Size: " << config.table_size
            << "\n# Maximum Read Size: " << config.maximum_read_size
            << "\n# Mutations per Request: " << config.mutations_per_request
            << "\n# Compiler: " << cloud_spanner::internal::CompilerId() << "-"
            << cloud_spanner::internal::CompilerVersion()
            << "\n# Build Flags: " << cloud_spanner::internal::BuildFlags()
            << "\n"
            << std::flush;

  using Runner = std::function<void(
      Config const&, cloud_spanner::Database const&, SampleSink const&)>;

  Runner runner = &SmokeTest;
  if (config.experiment != "smoke-test") {
    auto experiments = ListExperiments();
    auto entry = experiments.find(config.experiment);
    if (entry == experiments.end()) {
      std::cerr << "Unknown experiment " << config.experiment << "\n";
      return 1;
    }
    auto exp = entry->second;
    runner = [exp](Config const& config,
                   cloud_spanner::Database const& database,
                   SampleSink const& sink) {
      exp->SetUp(config, database);
      auto task = [config, database, sink, exp](int, int task_id) {
        cloud_spanner::Client client(cloud_spanner::MakeConnection(database));
        if (!config.shared_client) {
          std::string domain = "task:" + std::to_string(task_id);
          client = cloud_spanner::Client(cloud_spanner::MakeConnection(
              database,
              cloud_spanner::ConnectionOptions().set_channel_pool_domain(
                  domain)));
        }
        exp->Run(config, client, sink);
      };
      RunParallel(TaskCount(config), task);
    };
  }

  cloud_spanner::DatabaseAdminClient admin_client;
  auto created =
      admin_client.CreateDatabase(database, {R"sql(CREATE TABLE Singers (
                                SingerId   INT64 NOT NULL,
                                FirstName  STRING(1024),
                                LastName   STRING(1024)
                             ) PRIMARY KEY (SingerId))sql"});
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
  std::cout << "# Insert Throughput Results\n"
            << "Operation,Size,ElapsedTime,Success\n"
            << std::boolalpha;

  std::mutex cout_mu;
  auto cout_sink = [&cout_mu](std::vector<Sample> const& samples) mutable {
    std::unique_lock<std::mutex> lk(cout_mu);
    for (auto const& s : samples) {
      std::cout << OperationName(s.op) << ',' << s.size << ','
                << s.elapsed.count() << ',' << s.success << '\n';
    }
  };

  runner(config, database, cout_sink);

  auto drop = admin_client.DropDatabase(database);
  if (!drop.ok()) {
    std::cerr << "Error dropping database: " << drop << "\n";
  }
  std::cout << "# Experiment finished, database dropped\n";
  return 0;
}

namespace {
google::cloud::StatusOr<Config> ParseArgs(std::vector<std::string> args) {
  Config config;

  config.project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  config.instance_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_SPANNER_INSTANCE")
          .value_or("");

  config.experiment = "smoke-test";

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
      {"--duration=",
       [](Config& c, std::string const& v) {
         c.duration = std::chrono::seconds(std::stoi(v));
       }},
      {"--threads=",
       [](Config& c, std::string const& v) { c.threads = std::stoi(v); }},
      {"--threads-per-core=",
       [](Config& c, std::string const& v) {
         c.threads_per_core = std::stoi(v);
       }},
      {"--table-size=",
       [](Config& c, std::string const& v) { c.table_size = std::stol(v); }},
      {"--maximum-read-size=",
       [](Config& c, std::string const& v) {
         c.maximum_read_size = std::stoi(v);
       }},
      {"--mutations-per-request=",
       [](Config& c, std::string const& v) {
         c.mutations_per_request = std::stoi(v);
       }},
      {"--shared-client=",
       [](Config& c, std::string const& v) { c.shared_client = v == "true"; }},
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

  if (config.threads < 0) {
    return invalid_argument("The number of threads (" +
                            std::to_string(config.threads) + ") must be >= 0");
  }
  if (config.threads_per_core < 0) {
    return invalid_argument("The number of threads (" +
                            std::to_string(config.threads_per_core) +
                            ") must be >= 0");
  }
  if (config.threads_per_core < 0) {
    return invalid_argument("The number of threads (" +
                            std::to_string(config.threads_per_core) +
                            ") must be >= 0");
  }

  return config;
}

void RunParallel(int task_count, std::function<void(int, int)> const& task) {
  std::vector<std::future<void>> tasks(task_count);
  int count = 0;
  for (auto& t : tasks) {
    t = std::async(std::launch::async, task, task_count, count++);
  }
  for (auto& t : tasks) {
    t.get();
  }
}

}  // namespace
