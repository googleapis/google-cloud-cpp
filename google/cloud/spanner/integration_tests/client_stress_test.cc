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
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>
#include <future>
#include <random>
#include <thread>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

std::int64_t flag_table_size = 10 * 1000 * 1000;
std::int64_t flag_maximum_read_size = 10 * 1000;
std::chrono::seconds flag_duration(5);
int flag_threads = 0;  // 0 means use the threads_per_core setting.
int flag_threads_per_core = 4;

struct Result {
  std::int64_t failure_count = 0;
  std::int64_t success_count = 0;

  void Update(Status const& s) {
    if (!s.ok()) {
      ++failure_count;
    } else {
      ++success_count;
    }
  }

  Result operator+=(Result const& r) {
    failure_count += r.failure_count;
    success_count += r.success_count;
    return *this;
  }
};

int TaskCount() {
  if (flag_threads != 0) return flag_threads;
  auto cores = std::thread::hardware_concurrency();
  return cores == 0 ? flag_threads_per_core
                    : static_cast<int>(cores) * flag_threads_per_core;
}

class ClientStressTest : public spanner_testing::DatabaseIntegrationTest {};

/// @test Stress test the library using ExecuteQuery calls.
TEST_F(ClientStressTest, UpsertAndSelect) {
  int const task_count = TaskCount();

  auto select_task = [](Client client) {
    Result result{};

    // Each thread needs its own random bits generator.
    auto generator = google::cloud::internal::MakeDefaultPRNG();
    std::uniform_int_distribution<std::int64_t> random_key(0, flag_table_size);
    std::uniform_int_distribution<int> random_action(0, 1);
    std::uniform_int_distribution<std::int64_t> random_limit(
        0, flag_maximum_read_size);
    enum Action {
      kInsert = 0,
      kSelect = 1,
    };

    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + flag_duration;
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
  EXPECT_LE(total.failure_count, experiments_count * 0.001);
}

/// @test Stress test the library using Read calls.
TEST_F(ClientStressTest, UpsertAndRead) {
  int const task_count = TaskCount();

  auto read_task = [](Client client) {
    Result result{};

    // Each thread needs its own random bits generator.
    auto generator = google::cloud::internal::MakeDefaultPRNG();
    std::uniform_int_distribution<std::int64_t> random_key(0, flag_table_size);
    std::uniform_int_distribution<int> random_action(0, 1);
    std::uniform_int_distribution<std::int64_t> random_limit(
        0, flag_maximum_read_size);
    enum Action {
      kInsert = 0,
      kSelect = 1,
    };

    for (auto start = std::chrono::steady_clock::now(),
              deadline = start + flag_duration;
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
  EXPECT_LE(total.failure_count, experiments_count * 0.001);
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  ::testing::InitGoogleMock(&argc, argv);

  // TODO(#...) - refactor google-cloud-cpp code for command-line parsing.
  std::string const table_size_arg = "--table-size=";
  std::string const maximum_read_size_arg = "--maximum-read-size=";
  std::string const duration_arg = "--duration=";
  std::string const threads_arg = "--threads=";
  std::string const threads_per_core_arg = "--threads-per-core=";
  for (int i = 1; i != argc; ++i) {
    std::string arg = argv[i];
    if (arg.rfind(table_size_arg) == 0) {
      google::cloud::spanner::flag_table_size =
          std::stol(arg.substr(table_size_arg.size()));
    } else if (arg.rfind(maximum_read_size_arg) == 0) {
      google::cloud::spanner::flag_maximum_read_size =
          std::stoi(arg.substr(maximum_read_size_arg.size()));
    } else if (arg.rfind(threads_arg) == 0) {
      google::cloud::spanner::flag_threads =
          std::stoi(arg.substr(threads_arg.size()));
    } else if (arg.rfind(threads_per_core_arg) == 0) {
      google::cloud::spanner::flag_threads_per_core =
          std::stoi(arg.substr(threads_per_core_arg.size()));
    } else if (arg.rfind(duration_arg, 0) == 0) {
      google::cloud::spanner::flag_duration =
          std::chrono::seconds(std::stoi(arg.substr(duration_arg.size())));
    } else if (arg.rfind("--", 0) == 0) {
      std::cerr << "Unknown flag: " << arg << "\n";
    }
  }

  return RUN_ALL_TESTS();
}
