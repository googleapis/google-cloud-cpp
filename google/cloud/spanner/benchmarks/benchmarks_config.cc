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
#include "google/cloud/spanner/internal/compiler_info.h"
#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/getenv.h"
#include <functional>
#include <sstream>

namespace google {
namespace cloud {
namespace spanner_benchmarks {
inline namespace SPANNER_CLIENT_NS {

namespace spanner = ::google::cloud::spanner;

std::ostream& operator<<(std::ostream& os, Config const& config) {
  return os << "# Experiment: " << config.experiment
            << "\n# Project: " << config.project_id
            << "\n# Instance: " << config.instance_id
            << "\n# Database: " << config.database_id
            << "\n# Samples: " << config.samples
            << "\n# Minimum Threads: " << config.minimum_threads
            << "\n# Maximum Threads: " << config.maximum_threads
            << "\n# Minimum Clients/Channels: " << config.minimum_clients
            << "\n# Maximum Clients/Channels: " << config.maximum_clients
            << "\n# Iteration Duration: " << config.iteration_duration.count()
            << "s"
            << "\n# Table Size: " << config.table_size
            << "\n# Query Size: " << config.query_size
            << "\n# Use Only Stubs: " << config.use_only_stubs
            << "\n# Use Only Clients: " << config.use_only_clients
            << "\n# Compiler: " << spanner::internal::CompilerId() << "-"
            << spanner::internal::CompilerVersion()
            << "\n# Build Flags: " << google::cloud::internal::compiler_flags()
            << "\n";
}

google::cloud::StatusOr<Config> ParseArgs(std::vector<std::string> args) {
  Config config;

  config.experiment = "run-all";

  config.project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  auto emulator = google::cloud::internal::GetEnv("SPANNER_EMULATOR_HOST");
  if (!emulator.has_value()) {
    // When using the emulator it is easier to create an instance each time, and
    // PickRandomInstance() will do that for us. While we do not want to
    // benchmark the emulator, we do want to smoke test the benchmarks
    // themselves, and running them against the emulator is the faster way to do
    // so.
    config.instance_id = google::cloud::internal::GetEnv(
                             "GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID")
                             .value_or("");
  }

  struct Flag {
    std::string flag_name;
    std::function<void(Config&, std::string)> parser;
  };

  Flag flags[] = {
      {"--experiment=",
       [](Config& c, std::string v) { c.experiment = std::move(v); }},
      {"--project=",
       [](Config& c, std::string v) { c.project_id = std::move(v); }},
      {"--instance=",
       [](Config& c, std::string v) { c.instance_id = std::move(v); }},
      {"--database=",
       [](Config& c, std::string const& v) { c.database_id = std::move(v); }},
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
      // TODO(#1193) keep the `channels` flags and remove the `clients` aliases.
      {"--minimum-clients=",
       [](Config& c, std::string const& v) {
         c.minimum_clients = std::stoi(v);
       }},
      {"--minimum-channels=",
       [](Config& c, std::string const& v) {
         c.minimum_clients = std::stoi(v);
       }},
      {"--maximum-clients=",
       [](Config& c, std::string const& v) {
         c.maximum_clients = std::stoi(v);
       }},
      {"--maximum-channels=",
       [](Config& c, std::string const& v) {
         c.maximum_clients = std::stoi(v);
       }},
      {"--table-size=",
       [](Config& c, std::string const& v) { c.table_size = std::stoi(v); }},
      {"--query-size=",
       [](Config& c, std::string const& v) { c.query_size = std::stoi(v); }},

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

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_benchmarks
}  // namespace cloud
}  // namespace google
