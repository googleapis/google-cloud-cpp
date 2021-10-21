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
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/testing/database_integration_test.h"
#include "google/cloud/testing_util/command_line_parsing.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <future>
#include <random>
#include <thread>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {
using ::google::cloud::StatusOr;

auto constexpr kDescription = R"""(
A stress test for the Cloud Spanner C++ client library.

This experiment is largely a torture test for the library. The objective is to
detect bugs that escape unit and integration tests. Such tests are typically
short-lived and predictable, so we write a test that is long-lived and
unpredictable to find problems that would go otherwise unnoticed.

This test creates a number of threads testing `ExecuteQuery` and randomly
switching between insert and read calls.
)""";

struct Config {
  std::int64_t table_size = 10 * 1000 * 1000;
  std::int64_t maximum_read_size = 10 * 1000;

  std::chrono::seconds duration = std::chrono::seconds(5);

  int threads = 0;  // 0 means use the threads_per_core setting.
  int threads_per_core = 4;

  bool show_help = false;
};
Config config;

using ::google::cloud::testing_util::OptionDescriptor;
using ::google::cloud::testing_util::ParseDuration;

StatusOr<Config> ParseArgs(std::vector<std::string> args,
                           std::string const& description) {
  Config options;
  bool show_help = false;
  bool show_description = false;

  std::vector<OptionDescriptor> desc{
      {"--help", "print usage information",
       [&show_help](std::string const&) { show_help = true; }},
      {"--description", "print client stress test information",
       [&show_description](std::string const&) { show_description = true; }},
      {"--table-size", "the maximum size of each table key",
       [&options](std::string const& val) {
         options.table_size = std::stol(val);
       }},
      {"--maximum-read-size", "the maximum read size in each query",
       [&options](std::string const& val) {
         options.maximum_read_size = std::stol(val);
       }},
      {"--duration", "how long to run the test for in seconds",
       [&options](std::string const& val) {
         options.duration = ParseDuration(val);
       }},
      {"--threads", "if this is 0, threads-per-core is used instead",
       [&options](std::string const& val) {
         options.threads = std::stoi(val);
       }},
      {"--threads-per-core", "how many tasks to run simultaneously",
       [&options](std::string const& val) {
         options.threads_per_core = std::stoi(val);
       }}};
  auto const usage = BuildUsage(desc, args[0]);
  auto unparsed = OptionsParse(desc, args);

  if (show_description) {
    std::cout << description << "\n\n";
  }

  if (show_help) {
    std::cout << usage << "\n";
    options.show_help = true;
    return options;
  }

  return options;
}

struct Result {
  Status last_failure;
  std::int32_t failure_count = 0;
  std::int32_t success_count = 0;

  void Update(Status const& s) {
    if (!s.ok()) {
      last_failure = s;
      ++failure_count;
    } else {
      ++success_count;
    }
  }

  Result operator+=(Result const& r) {
    if (!r.last_failure.ok()) last_failure = r.last_failure;
    failure_count += r.failure_count;
    success_count += r.success_count;
    return *this;
  }
};

int TaskCount() {
  if (config.threads != 0) return config.threads;
  auto cores = std::thread::hardware_concurrency();
  return cores == 0 ? config.threads_per_core
                    : static_cast<int>(cores) * config.threads_per_core;
}

class ClientStressTest : public spanner_testing::DatabaseIntegrationTest {};

/// @test Test that ParseArgs works correctly.
TEST_F(ClientStressTest, ParseArgs) {
  std::string const cmd = "placeholder-cmd";
  auto config = ParseArgs({cmd, "--help"}, kDescription);
  EXPECT_TRUE(config && config->show_help);
  config = ParseArgs({cmd, "--description", "--help"}, kDescription);
  EXPECT_TRUE(config && config->show_help);
  config = ParseArgs({cmd, "--maximum-read-size=1000"}, kDescription);
  EXPECT_TRUE(config);
}

/// @test Stress test the library using ExecuteQuery calls.
TEST_F(ClientStressTest, UpsertAndSelect) {
  int const task_count = TaskCount();

  auto select_task = [](Client client) {
    Result result{};

    // Each thread needs its own random bits generator.
    auto generator = google::cloud::internal::MakeDefaultPRNG();
    std::uniform_int_distribution<std::int64_t> random_key(0,
                                                           config.table_size);
    std::uniform_int_distribution<int> random_action(0, 1);
    std::uniform_int_distribution<std::int64_t> random_limit(
        0, config.maximum_read_size);
    enum Action {
      kInsert = 0,
      kSelect = 1,
    };

    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto key = random_key(generator);
      auto action = static_cast<Action>(random_action(generator));

      if (action == kInsert) {
        auto s = std::to_string(key);
        auto commit =
            client.Commit(Mutations{spanner::MakeInsertOrUpdateMutation(
                "Singers", {"SingerId", "FirstName", "LastName"}, key,
                "fname-" + s, "lname-" + s)});
        result.Update(commit.status());
      } else {
        auto size = random_limit(generator);
        auto rows = client.ExecuteQuery(
            SqlStatement("SELECT SingerId, FirstName, LastName"
                         "  FROM Singers"
                         " WHERE SingerId >= @min"
                         "   AND SingerId <= @max",
                         {{"min", spanner::Value(key)},
                          {"max", spanner::Value(key + size)}}));
        for (auto const& row : rows) {
          result.Update(row.status());
        }
      }
    }
    return result;
  };

  Client client(MakeConnection(GetDatabase()));

  std::vector<std::future<Result>> tasks(task_count);
  for (auto& t : tasks) {
    t = std::async(std::launch::async, select_task, client);
  }

  Result total;
  for (auto& t : tasks) {
    total += t.get();
  }

  auto experiments_count = total.failure_count + total.success_count;
  EXPECT_LE(total.failure_count, experiments_count / 1000 + 1)
      << "failure_count=" << total.failure_count
      << ", success_count=" << total.success_count
      << ", last_failure=" << total.last_failure;
}

/// @test Stress test the library using Read calls.
TEST_F(ClientStressTest, UpsertAndRead) {
  int const task_count = TaskCount();

  auto read_task = [](Client client) {
    Result result{};

    // Each thread needs its own random bits generator.
    auto generator = google::cloud::internal::MakeDefaultPRNG();
    std::uniform_int_distribution<std::int64_t> random_key(0,
                                                           config.table_size);
    std::uniform_int_distribution<int> random_action(0, 1);
    std::uniform_int_distribution<std::int64_t> random_limit(
        0, config.maximum_read_size);
    enum Action {
      kInsert = 0,
      kSelect = 1,
    };

    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + config.duration;
         start < deadline; start = std::chrono::steady_clock::now()) {
      auto key = random_key(generator);
      auto action = static_cast<Action>(random_action(generator));

      if (action == kInsert) {
        auto s = std::to_string(key);
        auto commit =
            client.Commit(Mutations{spanner::MakeInsertOrUpdateMutation(
                "Singers", {"SingerId", "FirstName", "LastName"}, key,
                "fname-" + s, "lname-" + s)});
        result.Update(commit.status());
      } else {
        auto size = random_limit(generator);
        auto range =
            spanner::KeySet().AddRange(spanner::MakeKeyBoundClosed(key),
                                       spanner::MakeKeyBoundClosed(key + size));

        auto rows = client.Read("Singers", range,
                                {"SingerId", "FirstName", "LastName"});
        for (auto const& row : rows) {
          result.Update(row.status());
        }
      }
    }
    return result;
  };

  Client client(MakeConnection(GetDatabase()));

  std::vector<std::future<Result>> tasks(task_count);
  for (auto& t : tasks) {
    t = std::async(std::launch::async, read_task, client);
  }

  Result total;
  for (auto& t : tasks) {
    total += t.get();
  }

  auto experiments_count = total.failure_count + total.success_count;
  EXPECT_LE(total.failure_count, experiments_count / 1000 + 1)
      << "failure_count=" << total.failure_count
      << ", success_count=" << total.success_count
      << ", last_failure=" << total.last_failure;
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  namespace spanner = ::google::cloud::spanner;
  ::testing::InitGoogleMock(&argc, argv);

  auto config = spanner::ParseArgs({argv, argv + argc}, spanner::kDescription);
  if (!config) {
    std::cerr << "Error parsing command-line arguments\n";
    std::cerr << config.status() << "\n";
    return 1;
  }
  if (config->show_help) return 0;
  spanner::config = std::move(*config);

  return RUN_ALL_TESTS();
}
