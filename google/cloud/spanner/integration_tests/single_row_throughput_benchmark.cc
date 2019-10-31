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
#include <algorithm>
#include <future>
#include <random>
#include <sstream>
#include <thread>

namespace {

namespace cloud_spanner = google::cloud::spanner;

struct Config {
  std::string experiment;

  std::string project_id;
  std::string instance_id;

  int samples = 2;
  std::chrono::seconds iteration_duration = std::chrono::seconds(5);

  int minimum_threads = 1;
  int maximum_threads = 4;
  int minimum_clients = 1;
  int maximum_clients = 4;
  std::int64_t table_size = 10 * 1000 * 1000;
};

struct SingleRowThroughputSample {
  int client_count;
  int thread_count;
  int insert_count;
  std::chrono::microseconds elapsed;
};

using SampleSink = std::function<void(std::vector<SingleRowThroughputSample>)>;

class Experiment {
 public:
  virtual ~Experiment() = default;

  virtual void SetUp(Config const& config,
                     cloud_spanner::Database const& database) = 0;
  virtual void Run(Config const& config,
                   cloud_spanner::Database const& database,
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

  std::cout << std::boolalpha << "# Experiment: Single Row Throughput"
            << "\n# Project: " << config.project_id
            << "\n# Instance: " << config.instance_id
            << "\n# Database: " << database.database_id()
            << "\n# Samples: " << config.samples
            << "\n# Minimum Threads: " << config.minimum_threads
            << "\n# Maximum Threads: " << config.maximum_threads
            << "\n# Minimum Clients: " << config.minimum_clients
            << "\n# Maximum Clients: " << config.maximum_clients
            << "\n# Iteration Duration: " << config.iteration_duration.count()
            << "s"
            << "\n# Table Size: " << config.table_size
            << "\n# Compiler: " << cloud_spanner::internal::CompilerId() << "-"
            << cloud_spanner::internal::CompilerVersion()
            << "\n# Build Flags: " << cloud_spanner::internal::BuildFlags()
            << "\n"
            << std::flush;

  auto available = AvailableExperiments();
  auto e = available.find(config.experiment);
  if (e == available.end()) {
    std::cerr << "Experiment " << config.experiment << " not found\n";
    return 1;
  }

  cloud_spanner::DatabaseAdminClient admin_client;
  auto created =
      admin_client.CreateDatabase(database, {R"sql(CREATE TABLE KeyValue (
                                Key   INT64 NOT NULL,
                                Data  STRING(1024),
                             ) PRIMARY KEY (Key))sql"});
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

  std::cout << "ClientCount,ThreadCount,InsertCount,ElapsedTime\n"
            << std::flush;

  std::mutex cout_mu;
  auto cout_sink =
      [&cout_mu](
          std::vector<SingleRowThroughputSample> const& samples) mutable {
        std::unique_lock<std::mutex> lk(cout_mu);
        for (auto const& s : samples) {
          std::cout << std::boolalpha << s.client_count << ',' << s.thread_count
                    << ',' << s.insert_count << ',' << s.elapsed.count() << '\n'
                    << std::flush;
        }
      };

  auto experiment = e->second;
  experiment->SetUp(config, database);
  experiment->Run(config, database, cout_sink);

  auto drop = admin_client.DropDatabase(database);
  if (!drop.ok()) {
    std::cerr << "Error dropping database: " << drop << "\n";
  }
  std::cout << "# Experiment finished, database dropped\n";
  return 0;
}

namespace {

using RandomKeyGenerator = std::function<std::int64_t()>;
using ErrorSink = std::function<void(std::vector<google::cloud::Status>)>;

class InsertOrUpdateExperiment : public Experiment {
 public:
  void SetUp(Config const&, cloud_spanner::Database const&) override {}

  void Run(Config const& config, cloud_spanner::Database const& database,
           SampleSink const& sink) override {
    // Create enough clients for the worst case
    std::vector<cloud_spanner::Client> clients;
    std::cout << "# Creating clients " << std::flush;
    for (int i = 0; i != config.maximum_clients; ++i) {
      clients.emplace_back(cloud_spanner::Client(cloud_spanner::MakeConnection(
          database, cloud_spanner::ConnectionOptions().set_channel_pool_domain(
                        "task:" + std::to_string(i)))));
      std::cout << '.' << std::flush;
    }
    std::cout << " DONE\n";

    auto generator = google::cloud::internal::MakeDefaultPRNG();
    std::uniform_int_distribution<int> thread_count_gen(config.minimum_threads,
                                                        config.maximum_threads);

    for (int i = 0; i != config.samples; ++i) {
      auto const thread_count = thread_count_gen(generator);
      // TODO(#1000) - avoid deadlocks with more than 100 threads per client
      auto min_clients =
          (std::max)(thread_count / 100 + 1, config.minimum_clients);
      auto const client_count = std::uniform_int_distribution<std::size_t>(
          min_clients, clients.size() - 1)(generator);
      std::vector<cloud_spanner::Client> iteration_clients(
          clients.begin(), clients.begin() + client_count);
      RunIteration(config, iteration_clients, thread_count, sink, generator);
    }
  }

  void RunIteration(Config const& config,
                    std::vector<cloud_spanner::Client> const& clients,
                    int thread_count, SampleSink const& sink,
                    google::cloud::internal::DefaultPRNG generator) {
    std::mutex mu;
    std::uniform_int_distribution<std::int64_t> random_key(0,
                                                           config.table_size);
    RandomKeyGenerator locked_random_key = [&mu, &generator, &random_key] {
      std::lock_guard<std::mutex> lk(mu);
      return random_key(generator);
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
    int task_id = 0;
    for (auto& t : tasks) {
      auto client = clients[task_id++ % clients.size()];
      t = std::async(std::launch::async, &InsertOrUpdateExperiment::RunTask,
                     this, config, client, locked_random_key, error_sink);
    }
    int insert_count = 0;
    for (auto& t : tasks) {
      insert_count += t.get();
    }
    auto elapsed = std::chrono::steady_clock::now() - start;

    sink({SingleRowThroughputSample{
        static_cast<int>(clients.size()), thread_count, insert_count,
        std::chrono::duration_cast<std::chrono::microseconds>(elapsed)}});
  }

  int RunTask(Config const& config, cloud_spanner::Client client,
              RandomKeyGenerator const& key_generator,
              ErrorSink const& error_sink) {
    int count = 0;
    std::string value(1024, 'A');
    std::vector<google::cloud::Status> errors;
    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.iteration_duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto key = key_generator();
      auto m = cloud_spanner::MakeInsertOrUpdateMutation(
          "KeyValue", {"Key", "Data"}, key, value);
      auto result = client.Commit([&m](cloud_spanner::Transaction const&) {
        return cloud_spanner::Mutations{m};
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

std::map<std::string, std::shared_ptr<Experiment>> AvailableExperiments() {
  return {
      {"insert-or-update", std::make_shared<InsertOrUpdateExperiment>()},
  };
}

google::cloud::StatusOr<Config> ParseArgs(std::vector<std::string> args) {
  Config config;

  config.experiment = "insert-or-update";

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
  return config;
}

}  // namespace
