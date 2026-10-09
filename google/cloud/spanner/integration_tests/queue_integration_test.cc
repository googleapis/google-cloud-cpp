// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/mutations.h"
#include "google/cloud/spanner/testing/cleanup_stale_databases.h"
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/types/optional.h"
#include "google/spanner/admin/database/v1/spanner_database_admin.pb.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;

class QueueIntegrationTest
    : public ::google::cloud::testing_util::IntegrationTest {
 protected:
  static void SetUpTestSuite() {
    ::google::cloud::testing_util::IntegrationTest::SetUpTestSuite();
    if (internal::GetEnv("SPANNER_EMULATOR_HOST").has_value()) {
      GTEST_SKIP() << "Spanner Queues is not supported on the Emulator";
    }

    std::string const project_id =
        internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_THAT(project_id, Not(IsEmpty()));

    internal::DefaultPRNG generator = internal::MakeDefaultPRNG();
    StatusOr<std::string> instance_id = spanner_testing::PickRandomInstance(
        generator, project_id, "labels.edition:enterprise");
    if (!instance_id.ok()) {
      GTEST_SKIP() << "No Enterprise edition instance available: "
                   << instance_id.status();
    }

    std::string const database_id =
        spanner_testing::RandomDatabaseName(generator);
    db_.emplace(project_id, *instance_id, database_id);

    spanner_admin::DatabaseAdminClient admin_client(
        spanner_admin::MakeDatabaseAdminConnection());
    spanner_testing::CleanupStaleDatabases(
        admin_client, project_id, *instance_id,
        std::chrono::system_clock::now() - std::chrono::hours(7 * 24));

    google::spanner::admin::database::v1::CreateDatabaseRequest request;
    request.set_parent(db_->instance().FullName());
    request.set_create_statement("CREATE DATABASE `" + db_->database_id() +
                                 "`");
    request.add_extra_statements(R"sql(
      CREATE TABLE tasks (
        task_id INT64 NOT NULL,
        status STRING(64) NOT NULL
      ) PRIMARY KEY (task_id)
    )sql");
    request.add_extra_statements(R"sql(
      CREATE QUEUE testqueue (
        id INT64 NOT NULL,
        payload BYTES(MAX) NOT NULL,
      ) PRIMARY KEY (id),
      OPTIONS (receive_mode = 'PULL')
    )sql");

    StatusOr<google::spanner::admin::database::v1::Database> database =
        admin_client.CreateDatabase(request).get();
    if (!database.ok() &&
        (database.status().code() == StatusCode::kInvalidArgument ||
         database.status().code() == StatusCode::kUnimplemented)) {
      db_.reset();
      GTEST_SKIP() << "Spanner Queues DDL not enabled on target endpoint: "
                   << database.status();
    }
    ASSERT_THAT(database, IsOk());
  }

  static void TearDownTestSuite() {
    if (db_.has_value()) {
      spanner_admin::DatabaseAdminClient admin_client(
          spanner_admin::MakeDatabaseAdminConnection());
      Status drop_status = admin_client.DropDatabase(db_->FullName());
      EXPECT_THAT(drop_status, IsOk());
      db_.reset();
    }
    ::google::cloud::testing_util::IntegrationTest::TearDownTestSuite();
  }

  void SetUp() override {
    if (!db_.has_value()) {
      GTEST_SKIP() << "QueueIntegrationTest suite was skipped";
    }
  }

  void TearDown() override {
    if (db_.has_value()) {
      Client client(MakeConnection(*db_));
      RowStream rows =
          client.ExecuteQuery(SqlStatement("SELECT id FROM testqueue"));
      Mutations cleanup;
      for (StatusOr<std::tuple<std::int64_t>> const& row :
           StreamOf<std::tuple<std::int64_t>>(rows)) {
        if (row.ok()) {
          cleanup.push_back(
              MakeAckMutation("testqueue", MakeKey(std::get<0>(*row)), true));
        }
      }
      if (!cleanup.empty()) {
        (void)client.Commit(std::move(cleanup));
      }
    }
    ::google::cloud::testing_util::IntegrationTest::TearDown();
  }

  static Database const& GetDatabase() { return *db_; }

 private:
  static absl::optional<Database> db_;
};

absl::optional<Database> QueueIntegrationTest::db_;

TEST_F(QueueIntegrationTest, SendAndAckMutations) {
  Client client(MakeConnection(GetDatabase()));

  StatusOr<Timestamp> deliver_time =
      MakeTimestamp(std::chrono::system_clock::now());
  ASSERT_THAT(deliver_time, IsOk());

  Bytes const payload1(std::string("payload-1"));
  Bytes const payload2(std::string("payload-2"));

  // 1. Send immediate and scheduled messages via MakeSendMutation & Builder.
  StatusOr<CommitResult> send_commit = client.Commit(Mutations{
      MakeSendMutation("testqueue", MakeKey(std::int64_t{1}), Value(payload1)),
      SendMutationBuilder("testqueue", MakeKey(std::int64_t{2}),
                          Value(payload2))
          .SetDeliverTime(*deliver_time)
          .Build(),
  });
  ASSERT_THAT(send_commit, IsOk());

  // Verify messages exist in testqueue.
  std::vector<std::pair<std::int64_t, Bytes>> read_rows;
  RowStream query_rows = client.ExecuteQuery(
      SqlStatement("SELECT id, payload FROM testqueue ORDER BY id"));
  for (StatusOr<std::tuple<std::int64_t, Bytes>> const& row :
       StreamOf<std::tuple<std::int64_t, Bytes>>(query_rows)) {
    ASSERT_THAT(row, IsOk());
    read_rows.emplace_back(std::get<0>(*row), std::get<1>(*row));
  }
  EXPECT_THAT(read_rows, ElementsAre(Pair(std::int64_t{1}, payload1),
                                     Pair(std::int64_t{2}, payload2)));

  // 2. Ack existing messages.
  StatusOr<CommitResult> ack_commit = client.Commit(Mutations{
      MakeAckMutation("testqueue", MakeKey(std::int64_t{1})),
      MakeAckMutation("testqueue", MakeKey(std::int64_t{2})),
  });
  ASSERT_THAT(ack_commit, IsOk());

  std::vector<std::int64_t> remaining_ids;
  RowStream remaining_stream =
      client.ExecuteQuery(SqlStatement("SELECT id FROM testqueue"));
  for (StatusOr<std::tuple<std::int64_t>> const& row :
       StreamOf<std::tuple<std::int64_t>>(remaining_stream)) {
    ASSERT_THAT(row, IsOk());
    remaining_ids.push_back(std::get<0>(*row));
  }
  EXPECT_THAT(remaining_ids, IsEmpty());

  // 3. Ack missing key with ignore_not_found = true succeeds.
  StatusOr<CommitResult> ack_ignored = client.Commit(Mutations{
      MakeAckMutation("testqueue", MakeKey(std::int64_t{999}), true),
  });
  EXPECT_THAT(ack_ignored, IsOk());

  // 4. Ack missing key with default ignore_not_found = false fails with
  // kNotFound.
  StatusOr<CommitResult> ack_missing = client.Commit(Mutations{
      MakeAckMutation("testqueue", MakeKey(std::int64_t{999})),
  });
  EXPECT_THAT(ack_missing, StatusIs(StatusCode::kNotFound));
}

TEST_F(QueueIntegrationTest, EndToEndTransactionalWorkflow) {
  Client client(MakeConnection(GetDatabase()));
  std::int64_t constexpr kTaskId = 1001;
  Bytes const expected_payload(std::string("process-order-1001"));

  // 1. Producer: atomically insert task row and send queue message.
  StatusOr<CommitResult> produce_commit = client.Commit(Mutations{
      MakeInsertMutation("tasks", {"task_id", "status"}, kTaskId, "pending"),
      SendMutationBuilder("testqueue", MakeKey(kTaskId),
                          Value(expected_payload))
          .Build(),
  });
  ASSERT_THAT(produce_commit, IsOk());

  // 2. Consumer: receive message and lease token via receive_<queue> TVF.
  RowStream receive_stream = client.ExecuteQuery(
      SqlStatement("SELECT id, payload, spanner_lease_token "
                   "FROM spanner.receive_testqueue(max_batch_size => 1)"));
  auto receive_it = receive_stream.begin();
  ASSERT_TRUE(receive_it != receive_stream.end());
  if (!receive_it->ok() &&
      (receive_it->status().code() == StatusCode::kInvalidArgument ||
       receive_it->status().code() == StatusCode::kUnimplemented)) {
    GTEST_SKIP()
        << "Spanner Queue consumer TVF not enabled on target endpoint: "
        << receive_it->status();
  }
  ASSERT_THAT(*receive_it, IsOk());
  StatusOr<std::tuple<std::int64_t, Bytes, Bytes>> received =
      (*receive_it)->get<std::tuple<std::int64_t, Bytes, Bytes>>();
  ASSERT_THAT(received, IsOk());
  EXPECT_EQ(std::get<0>(*received), kTaskId);
  EXPECT_EQ(std::get<1>(*received), expected_payload);
  Bytes const lease_token = std::get<2>(*received);
  EXPECT_THAT(lease_token.get<std::string>(), Not(IsEmpty()));

  // 3. Extend lease via renewlease_<queue> TVF.
  RowStream renew_stream = client.ExecuteQuery(
      SqlStatement("SELECT * FROM spanner.renewlease_testqueue([@lease_token])",
                   {{"lease_token", Value(lease_token)}}));
  for (StatusOr<Row> const& row : renew_stream) {
    ASSERT_THAT(row, IsOk());
  }

  // 4. Worker completion: atomically update task row and ack queue message.
  StatusOr<CommitResult> complete_commit = client.Commit(Mutations{
      MakeUpdateMutation("tasks", {"task_id", "status"}, kTaskId, "done"),
      AckMutationBuilder("testqueue", MakeKey(kTaskId)).Build(),
  });
  ASSERT_THAT(complete_commit, IsOk());

  // Verify queue is empty and task status is done.
  RowStream verify_queue =
      client.ExecuteQuery(SqlStatement("SELECT id FROM testqueue"));
  EXPECT_TRUE(verify_queue.begin() == verify_queue.end());

  RowStream verify_task = client.ExecuteQuery(
      SqlStatement("SELECT status FROM tasks WHERE task_id = @id",
                   {{"id", Value(kTaskId)}}));
  auto task_it = verify_task.begin();
  ASSERT_TRUE(task_it != verify_task.end());
  ASSERT_THAT(*task_it, IsOk());
  EXPECT_THAT((*task_it)->get<std::string>(0), IsOkAndHolds("done"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
