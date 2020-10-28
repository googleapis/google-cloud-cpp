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
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/time/time.h"
#include <gmock/gmock.h>
#include <future>
#include <random>
#include <thread>

ABSL_FLAG(std::int64_t, table_size, 10 * 1000 * 1000, "");
ABSL_FLAG(std::int64_t, maximum_read_size, 10 * 1000, "");
ABSL_FLAG(absl::Duration, duration, absl::Seconds(5), "");
ABSL_FLAG(int, threads, 0, "0 means use the threads_per_core setting");
ABSL_FLAG(int, threads_per_core, 4, "");

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

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
  if (absl::GetFlag(FLAGS_threads) != 0) return absl::GetFlag(FLAGS_threads);
  auto cores = std::thread::hardware_concurrency();
  return cores == 0
             ? absl::GetFlag(FLAGS_threads_per_core)
             : static_cast<int>(cores) * absl::GetFlag(FLAGS_threads_per_core);
}

class ClientStressTest : public spanner_testing::DatabaseIntegrationTest {};

/// @test Stress test the library using ExecuteQuery calls.
TEST_F(ClientStressTest, UpsertAndSelect) {
  int const task_count = TaskCount();

  auto select_task = [](Client client) {
    Result result{};

    // Each thread needs its own random bits generator.
    auto generator = google::cloud::internal::MakeDefaultPRNG();
    std::uniform_int_distribution<std::int64_t> random_key(
        0, absl::GetFlag(FLAGS_table_size));
    std::uniform_int_distribution<int> random_action(0, 1);
    std::uniform_int_distribution<std::int64_t> random_limit(
        0, absl::GetFlag(FLAGS_maximum_read_size));
    enum Action {
      kInsert = 0,
      kSelect = 1,
    };

    for (auto start = absl::Now(),
              deadline = start + absl::GetFlag(FLAGS_duration);
         start < deadline; start = absl::Now()) {
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
    std::uniform_int_distribution<std::int64_t> random_key(
        0, absl::GetFlag(FLAGS_table_size));
    std::uniform_int_distribution<int> random_action(0, 1);
    std::uniform_int_distribution<std::int64_t> random_limit(
        0, absl::GetFlag(FLAGS_maximum_read_size));
    enum Action {
      kInsert = 0,
      kSelect = 1,
    };

    for (auto start = absl::Now(),
              deadline = start + absl::GetFlag(FLAGS_duration);
         start < deadline; start = absl::Now()) {
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
  ::testing::InitGoogleMock(&argc, argv);

  absl::ParseCommandLine(argc, argv);

  return RUN_ALL_TESTS();
}
