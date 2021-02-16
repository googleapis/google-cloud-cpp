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

#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/mocks/mock_database_admin_connection.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::google::cloud::spanner_mocks::MockDatabaseAdminConnection;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
namespace gcsa = ::google::spanner::admin::database::v1;

/// @test Verify DatabaseAdminClient uses CreateDatabase() correctly.
TEST(DatabaseAdminClientTest, CreateDatabase) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();

  Database dbase("test-project", "test-instance", "test-db");

  EXPECT_CALL(*mock, CreateDatabase(_))
      .WillOnce(
          [&dbase](DatabaseAdminConnection::CreateDatabaseParams const& p) {
            EXPECT_EQ(p.database, dbase);
            EXPECT_THAT(p.extra_statements, ElementsAre("-- NOT SQL for test"));
            gcsa::Database database;
            database.set_name(dbase.FullName());
            database.set_state(gcsa::Database::CREATING);
            return make_ready_future(make_status_or(database));
          });

  DatabaseAdminClient client(std::move(mock));
  auto fut = client.CreateDatabase(dbase, {"-- NOT SQL for test"});
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
  auto db = fut.get();
  EXPECT_STATUS_OK(db);

  EXPECT_EQ(dbase.FullName(), db->name());
  EXPECT_EQ(gcsa::Database::CREATING, db->state());
}

/// @test Verify DatabaseAdminClient uses GetDatabase() correctly.
TEST(DatabaseAdminClientTest, GetDatabase) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  Database dbase("test-project", "test-instance", "test-db");

  EXPECT_CALL(*mock, GetDatabase(_))
      .WillOnce([&dbase](DatabaseAdminConnection::GetDatabaseParams const& p) {
        EXPECT_EQ(dbase, p.database);
        gcsa::Database response;
        response.set_name(p.database.FullName());
        response.set_state(gcsa::Database::READY);
        return response;
      });

  DatabaseAdminClient client(std::move(mock));
  auto response = client.GetDatabase(dbase);
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(gcsa::Database::READY, response->state());
  EXPECT_EQ(dbase.FullName(), response->name());
}

/// @test Verify DatabaseAdminClient uses GetDatabaseDdl() correctly.
TEST(DatabaseAdminClientTest, GetDatabaseDdl) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  Database const expected_name("test-project", "test-instance",
                               "test-database");

  EXPECT_CALL(*mock, GetDatabaseDdl(_))
      .WillOnce([&expected_name](
                    DatabaseAdminConnection::GetDatabaseDdlParams const& p) {
        EXPECT_EQ(expected_name, p.database);
        gcsa::GetDatabaseDdlResponse response;
        response.add_statements("CREATE DATABASE test-database");
        return response;
      });

  DatabaseAdminClient client(std::move(mock));
  auto response = client.GetDatabaseDdl(expected_name);
  EXPECT_STATUS_OK(response);
  ASSERT_EQ(1, response->statements_size());
  ASSERT_EQ("CREATE DATABASE test-database", response->statements(0));
}

/// @test Verify DatabaseAdminClient uses UpdateDatabase() correctly.
TEST(DatabaseAdminClientTest, UpdateDatabase) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();

  Database dbase("test-project", "test-instance", "test-db");

  EXPECT_CALL(*mock, UpdateDatabase(_))
      .WillOnce(
          [&dbase](DatabaseAdminConnection::UpdateDatabaseParams const& p) {
            EXPECT_EQ(p.database, dbase);
            EXPECT_THAT(p.statements, ElementsAre("-- test only: NOT SQL"));
            gcsa::UpdateDatabaseDdlMetadata metadata;
            metadata.add_statements("-- test only: NOT SQL");
            return make_ready_future(make_status_or(metadata));
          });

  DatabaseAdminClient client(std::move(mock));
  auto fut = client.UpdateDatabase(dbase, {"-- test only: NOT SQL"});
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
  auto db = fut.get();
  EXPECT_STATUS_OK(db);

  EXPECT_THAT(db->statements(), ElementsAre("-- test only: NOT SQL"));
}

TEST(DatabaseAdminClientTest, ListDatabases) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  Instance const expected_instance("test-project", "test-instance");
  EXPECT_CALL(*mock, ListDatabases(_))
      .WillOnce([&expected_instance](
                    DatabaseAdminConnection::ListDatabasesParams const& p) {
        EXPECT_EQ(expected_instance, p.instance);

        return google::cloud::internal::MakePaginationRange<ListDatabaseRange>(
            gcsa::ListDatabasesRequest{},
            [](gcsa::ListDatabasesRequest const&) {
              return StatusOr<gcsa::ListDatabasesResponse>(
                  Status(StatusCode::kPermissionDenied, "uh-oh"));
            },
            [](gcsa::ListDatabasesResponse const&) {
              return std::vector<gcsa::Database>{};
            });
      });

  DatabaseAdminClient client(mock);
  auto range = client.ListDatabases(expected_instance);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

/// @test Verify DatabaseAdminClient uses GetIamPolicy() correctly.
TEST(DatabaseAdminClientTest, GetIamPolicy) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  Database const expected_db("test-project", "test-instance", "test-database");
  std::string const expected_role = "roles/spanner.databaseReader";
  std::string const expected_member = "user:foobar@example.com";
  EXPECT_CALL(*mock, GetIamPolicy(_))
      .WillOnce([&expected_db, &expected_role, &expected_member](
                    DatabaseAdminConnection::GetIamPolicyParams const& p) {
        EXPECT_EQ(expected_db, p.database);
        google::iam::v1::Policy response;
        auto& binding = *response.add_bindings();
        binding.set_role(expected_role);
        *binding.add_members() = expected_member;
        return response;
      });

  DatabaseAdminClient client(std::move(mock));
  auto response = client.GetIamPolicy(expected_db);
  EXPECT_STATUS_OK(response);
  ASSERT_EQ(1, response->bindings().size());
  ASSERT_EQ(expected_role, response->bindings().Get(0).role());
  ASSERT_EQ(1, response->bindings().Get(0).members().size());
  ASSERT_EQ(expected_member, response->bindings().Get(0).members().Get(0));
}

/// @test Verify DatabaseAdminClient uses SetIamPolicy() correctly.
TEST(DatabaseAdminClientTest, SetIamPolicy) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  Database const expected_db("test-project", "test-instance", "test-database");
  EXPECT_CALL(*mock, SetIamPolicy(_))
      .WillOnce(
          [&expected_db](DatabaseAdminConnection::SetIamPolicyParams const& p) {
            EXPECT_EQ(expected_db, p.database);
            return p.policy;
          });
  DatabaseAdminClient client(std::move(mock));
  auto response = client.SetIamPolicy(expected_db, google::iam::v1::Policy{});
  EXPECT_STATUS_OK(response);
}

TEST(DatabaseAdminClientTest, SetIamPolicyOccGetFailure) {
  Database const db("test-project", "test-instance", "test-database");
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  EXPECT_CALL(*mock, GetIamPolicy(_))
      .WillOnce([&db](DatabaseAdminConnection::GetIamPolicyParams const& p) {
        EXPECT_EQ(db, p.database);
        return Status(StatusCode::kPermissionDenied, "uh-oh");
      });

  DatabaseAdminClient client(mock);
  auto actual = client.SetIamPolicy(db, [](google::iam::v1::Policy const&) {
    return absl::optional<google::iam::v1::Policy>{};
  });
  EXPECT_THAT(actual, StatusIs(StatusCode::kPermissionDenied));
}

TEST(DatabaseAdminClientTest, SetIamPolicyOccNoUpdates) {
  Database const db("test-project", "test-instance", "test-database");
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  EXPECT_CALL(*mock, GetIamPolicy(_))
      .WillOnce([&db](DatabaseAdminConnection::GetIamPolicyParams const& p) {
        EXPECT_EQ(db, p.database);
        google::iam::v1::Policy r;
        r.set_etag("test-etag");
        return r;
      });
  EXPECT_CALL(*mock, SetIamPolicy(_)).Times(0);

  DatabaseAdminClient client(mock);
  auto actual = client.SetIamPolicy(db, [](google::iam::v1::Policy const& p) {
    EXPECT_EQ("test-etag", p.etag());
    return absl::optional<google::iam::v1::Policy>{};
  });
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ("test-etag", actual->etag());
}

std::unique_ptr<TransactionRerunPolicy> RerunPolicyForTesting() {
  return LimitedErrorCountTransactionRerunPolicy(/*maximum_failures=*/3)
      .clone();
}

std::unique_ptr<BackoffPolicy> BackoffPolicyForTesting() {
  return ExponentialBackoffPolicy(
             /*initial_delay=*/std::chrono::microseconds(1),
             /*maximum_delay=*/std::chrono::microseconds(1), /*scaling=*/2.0)
      .clone();
}

TEST(DatabaseAdminClientTest, SetIamPolicyOccRetryAborted) {
  Database const db("test-project", "test-instance", "test-database");
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  EXPECT_CALL(*mock, GetIamPolicy(_))
      .WillOnce([&db](DatabaseAdminConnection::GetIamPolicyParams const& p) {
        EXPECT_EQ(db, p.database);
        google::iam::v1::Policy r;
        r.set_etag("test-etag-1");
        return r;
      })
      .WillOnce([&db](DatabaseAdminConnection::GetIamPolicyParams const& p) {
        EXPECT_EQ(db, p.database);
        google::iam::v1::Policy r;
        r.set_etag("test-etag-2");
        return r;
      });
  EXPECT_CALL(*mock, SetIamPolicy(_))
      .WillOnce([&db](DatabaseAdminConnection::SetIamPolicyParams const& p) {
        EXPECT_EQ(db, p.database);
        EXPECT_EQ("test-etag-1", p.policy.etag());
        return Status(StatusCode::kAborted, "aborted");
      })
      .WillOnce([&db](DatabaseAdminConnection::SetIamPolicyParams const& p) {
        EXPECT_EQ(db, p.database);
        EXPECT_EQ("test-etag-2", p.policy.etag());
        google::iam::v1::Policy r;
        r.set_etag("test-etag-3");
        return r;
      });

  DatabaseAdminClient client(mock);
  int counter = 0;
  auto actual = client.SetIamPolicy(
      db,
      [&counter](google::iam::v1::Policy p) {
        EXPECT_EQ("test-etag-" + std::to_string(++counter), p.etag());
        return p;
      },
      RerunPolicyForTesting(), BackoffPolicyForTesting());
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ("test-etag-3", actual->etag());
}

TEST(DatabaseAdminClientTest, SetIamPolicyOccRetryAbortedTooManyFailures) {
  Database const db("test-project", "test-instance", "test-database");
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  EXPECT_CALL(*mock, GetIamPolicy(_))
      .WillRepeatedly(
          [&db](DatabaseAdminConnection::GetIamPolicyParams const& p) {
            EXPECT_EQ(db, p.database);
            google::iam::v1::Policy r;
            r.set_etag("test-etag-1");
            return r;
          });
  EXPECT_CALL(*mock, SetIamPolicy(_))
      .Times(AtLeast(2))
      .WillRepeatedly(
          [&db](DatabaseAdminConnection::SetIamPolicyParams const& p) {
            EXPECT_EQ(db, p.database);
            EXPECT_EQ("test-etag-1", p.policy.etag());
            return Status(StatusCode::kAborted, "test-msg");
          });

  DatabaseAdminClient client(mock);
  auto actual = client.SetIamPolicy(
      db, [](google::iam::v1::Policy p) { return p; }, RerunPolicyForTesting(),
      BackoffPolicyForTesting());
  EXPECT_THAT(actual, StatusIs(StatusCode::kAborted, HasSubstr("test-msg")));
}

/// @test Verify DatabaseAdminClient uses TestIamPermissions() correctly.
TEST(DatabaseAdminClientTest, TestIamPermissions) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  Database const expected_db("test-project", "test-instance", "test-database");
  std::string expected_permission = "spanner.databases.read";
  EXPECT_CALL(*mock, TestIamPermissions(_))
      .WillOnce(
          [&expected_db, &expected_permission](
              DatabaseAdminConnection::TestIamPermissionsParams const& p) {
            EXPECT_EQ(expected_db, p.database);
            EXPECT_EQ(1, p.permissions.size());
            EXPECT_EQ(expected_permission, p.permissions.at(0));
            google::iam::v1::TestIamPermissionsResponse response;
            response.add_permissions(expected_permission);
            return response;
          });
  DatabaseAdminClient client(std::move(mock));
  auto response = client.TestIamPermissions(expected_db, {expected_permission});
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(1, response->permissions_size());
  EXPECT_EQ(expected_permission, response->permissions(0));
}

/// @test Verify DatabaseAdminClient uses CreateBackup() correctly.
TEST(DatabaseAdminClientTest, CreateBackup) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();

  Database dbase("test-project", "test-instance", "test-db");
  std::string backup_id = "test-backup";
  auto now = absl::Now();
  auto expire_time = MakeTimestamp(now + absl::Hours(7)).value();
  auto version_time = MakeTimestamp(now - absl::Hours(7)).value();
  Backup backup_name(dbase.instance(), backup_id);
  EXPECT_CALL(*mock, CreateBackup(_))
      .WillOnce([&dbase, &expire_time, &version_time, &backup_id, &backup_name](
                    DatabaseAdminConnection::CreateBackupParams const& p) {
        EXPECT_EQ(p.database, dbase);
        EXPECT_EQ(MakeTimestamp(p.expire_time).value(),
                  MakeTimestamp(
                      expire_time.get<std::chrono::system_clock::time_point>()
                          .value())
                      .value());
        EXPECT_EQ(p.expire_timestamp, expire_time);
        EXPECT_EQ(p.version_time, version_time);
        EXPECT_EQ(p.backup_id, backup_id);
        gcsa::Backup backup;
        backup.set_name(backup_name.FullName());
        backup.set_state(gcsa::Backup::CREATING);
        return make_ready_future(make_status_or(backup));
      })
      .WillOnce([&dbase, &expire_time, &backup_id, &backup_name](
                    DatabaseAdminConnection::CreateBackupParams const& p) {
        EXPECT_EQ(p.database, dbase);
        EXPECT_EQ(MakeTimestamp(p.expire_time).value(),
                  MakeTimestamp(
                      expire_time.get<std::chrono::system_clock::time_point>()
                          .value())
                      .value());
        EXPECT_EQ(p.expire_timestamp,
                  MakeTimestamp(
                      expire_time.get<std::chrono::system_clock::time_point>()
                          .value())
                      .value());  // loss of precision
        EXPECT_FALSE(p.version_time.has_value());
        EXPECT_EQ(p.backup_id, backup_id);
        gcsa::Backup backup;
        backup.set_name(backup_name.FullName());
        backup.set_state(gcsa::Backup::CREATING);
        return make_ready_future(make_status_or(backup));
      });

  DatabaseAdminClient client(std::move(mock));
  auto fut = client.CreateBackup(dbase, backup_id, expire_time, version_time);
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
  auto backup = fut.get();
  EXPECT_STATUS_OK(backup);
  EXPECT_EQ(backup_name.FullName(), backup->name());
  EXPECT_EQ(gcsa::Backup::CREATING, backup->state());

  // Exercise the old interface with just a `time_point` expiration parameter.
  fut = client.CreateBackup(
      dbase, backup_id,
      expire_time.get<std::chrono::system_clock::time_point>().value());
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
  backup = fut.get();
  EXPECT_STATUS_OK(backup);
  EXPECT_EQ(backup_name.FullName(), backup->name());
  EXPECT_EQ(gcsa::Backup::CREATING, backup->state());
}

/// @test Verify DatabaseAdminClient uses RestoreDatabase() correctly.
TEST(DatabaseAdminClientTest, RestoreDatabase) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();

  Database dbase("test-project", "test-instance", "test-db");
  Backup backup(dbase.instance(), "test-backup");
  EXPECT_CALL(*mock, RestoreDatabase(_))
      .WillOnce([&dbase, &backup](
                    DatabaseAdminConnection::RestoreDatabaseParams const& p) {
        EXPECT_EQ(p.database, dbase);
        EXPECT_EQ(p.backup_full_name, backup.FullName());
        gcsa::Database database;
        database.set_name(dbase.FullName());
        database.set_state(gcsa::Database::READY_OPTIMIZING);
        return make_ready_future(make_status_or(database));
      });

  DatabaseAdminClient client(std::move(mock));
  auto fut = client.RestoreDatabase(dbase, backup);
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
  auto database = fut.get();
  EXPECT_STATUS_OK(database);

  EXPECT_EQ(dbase.FullName(), database->name());
  EXPECT_EQ(gcsa::Database::READY_OPTIMIZING, database->state());
}

/// @test Verify DatabaseAdminClient uses RestoreDatabase() correctly.
TEST(DatabaseAdminClientTest, RestoreDatabaseOverload) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();

  Database dbase("test-project", "test-instance", "test-db");
  Backup backup_name(dbase.instance(), "test-backup");
  gcsa::Backup backup;
  backup.set_name(backup_name.FullName());
  EXPECT_CALL(*mock, RestoreDatabase(_))
      .WillOnce([&dbase, &backup_name](
                    DatabaseAdminConnection::RestoreDatabaseParams const& p) {
        EXPECT_EQ(p.database, dbase);
        EXPECT_EQ(p.backup_full_name, backup_name.FullName());
        gcsa::Database database;
        database.set_name(dbase.FullName());
        database.set_state(gcsa::Database::READY_OPTIMIZING);
        return make_ready_future(make_status_or(database));
      });

  DatabaseAdminClient client(std::move(mock));
  auto fut = client.RestoreDatabase(dbase, backup);
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
  auto database = fut.get();
  EXPECT_STATUS_OK(database);

  EXPECT_EQ(dbase.FullName(), database->name());
  EXPECT_EQ(gcsa::Database::READY_OPTIMIZING, database->state());
}

/// @test Verify DatabaseAdminClient uses GetBackup() correctly.
TEST(DatabaseAdminClientTest, GetBackup) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  Backup backup(Instance("test-project", "test-instance"), "test-backup");

  EXPECT_CALL(*mock, GetBackup(_))
      .WillOnce([&backup](DatabaseAdminConnection::GetBackupParams const& p) {
        EXPECT_EQ(backup.FullName(), p.backup_full_name);
        gcsa::Backup response;
        response.set_name(p.backup_full_name);
        response.set_state(gcsa::Backup::READY);
        return response;
      });

  DatabaseAdminClient client(std::move(mock));
  auto response = client.GetBackup(backup);
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(gcsa::Backup::READY, response->state());
  EXPECT_EQ(backup.FullName(), response->name());
}

/// @test Verify DatabaseAdminClient uses DeleteBackup() correctly.
TEST(DatabaseAdminClientTest, DeleteBackup) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  Backup backup(Instance("test-project", "test-instance"), "test-backup");

  EXPECT_CALL(*mock, DeleteBackup(_))
      .WillOnce(
          [&backup](DatabaseAdminConnection::DeleteBackupParams const& p) {
            EXPECT_EQ(backup.FullName(), p.backup_full_name);
            return Status();
          });

  DatabaseAdminClient client(std::move(mock));
  auto response = client.DeleteBackup(backup);
  EXPECT_STATUS_OK(response);
}

/// @test Verify DatabaseAdminClient uses DeleteBackup() correctly.
TEST(DatabaseAdminClientTest, DeleteBackupOverload) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  Backup backup_name(Instance("test-project", "test-instance"), "test-backup");
  gcsa::Backup backup;
  backup.set_name(backup_name.FullName());

  EXPECT_CALL(*mock, DeleteBackup(_))
      .WillOnce(
          [&backup_name](DatabaseAdminConnection::DeleteBackupParams const& p) {
            EXPECT_EQ(backup_name.FullName(), p.backup_full_name);
            return Status();
          });

  DatabaseAdminClient client(std::move(mock));
  auto response = client.DeleteBackup(backup);
  EXPECT_STATUS_OK(response);
}

TEST(DatabaseAdminClientTest, ListBackups) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  Instance const expected_instance("test-project", "test-instance");
  std::string expected_filter("test-filter");
  EXPECT_CALL(*mock, ListBackups(_))
      .WillOnce([&expected_instance, &expected_filter](
                    DatabaseAdminConnection::ListBackupsParams const& p) {
        EXPECT_EQ(expected_instance, p.instance);
        EXPECT_EQ(expected_filter, p.filter);

        return google::cloud::internal::MakePaginationRange<ListBackupsRange>(
            gcsa::ListBackupsRequest{},
            [](gcsa::ListBackupsRequest const&) {
              return StatusOr<gcsa::ListBackupsResponse>(
                  Status(StatusCode::kPermissionDenied, "uh-oh"));
            },
            [](gcsa::ListBackupsResponse const&) {
              return std::vector<gcsa::Backup>{};
            });
      });

  DatabaseAdminClient client(mock);
  auto range = client.ListBackups(expected_instance, expected_filter);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

/// @test Verify DatabaseAdminClient uses GetBackup() correctly.
TEST(DatabaseAdminClientTest, UpdateBackupExpireTime) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  Backup backup(Instance("test-project", "test-instance"), "test-backup");
  auto expire_time = MakeTimestamp(absl::Now() + absl::Hours(7)).value();

  EXPECT_CALL(*mock, UpdateBackup(_))
      .WillOnce([&backup, &expire_time](
                    DatabaseAdminConnection::UpdateBackupParams const& p) {
        EXPECT_EQ(backup.FullName(), p.request.backup().name());
        EXPECT_THAT(expire_time,
                    MakeTimestamp(p.request.backup().expire_time()).value());
        gcsa::Backup response;
        response.set_name(p.request.backup().name());
        *response.mutable_expire_time() = p.request.backup().expire_time();
        response.set_state(gcsa::Backup::READY);
        return response;
      })
      .WillOnce([&backup, &expire_time](
                    DatabaseAdminConnection::UpdateBackupParams const& p) {
        EXPECT_EQ(backup.FullName(), p.request.backup().name());
        EXPECT_THAT(MakeTimestamp(
                        expire_time.get<std::chrono::system_clock::time_point>()
                            .value())
                        .value(),  // loss of precision
                    MakeTimestamp(p.request.backup().expire_time()).value());
        gcsa::Backup response;
        response.set_name(p.request.backup().name());
        *response.mutable_expire_time() = p.request.backup().expire_time();
        response.set_state(gcsa::Backup::READY);
        return response;
      });

  DatabaseAdminClient client(std::move(mock));
  auto response = client.UpdateBackupExpireTime(backup, expire_time);
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(gcsa::Backup::READY, response->state());
  EXPECT_EQ(backup.FullName(), response->name());
  EXPECT_THAT(expire_time, MakeTimestamp(response->expire_time()).value());

  // Exercise the old interface with a `time_point` expiration parameter.
  response = client.UpdateBackupExpireTime(
      backup, expire_time.get<std::chrono::system_clock::time_point>().value());
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(gcsa::Backup::READY, response->state());
  EXPECT_EQ(backup.FullName(), response->name());
  EXPECT_THAT(
      MakeTimestamp(
          expire_time.get<std::chrono::system_clock::time_point>().value())
          .value(),  // loss of precision
      MakeTimestamp(response->expire_time()).value());
}

/// @test Verify DatabaseAdminClient uses GetBackup() correctly.
TEST(DatabaseAdminClientTest, UpdateBackupExpireTimeOverload) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  Backup backup_name(Instance("test-project", "test-instance"), "test-backup");
  gcsa::Backup backup;
  backup.set_name(backup_name.FullName());
  auto expire_time = MakeTimestamp(absl::Now() + absl::Hours(7)).value();

  EXPECT_CALL(*mock, UpdateBackup(_))
      .WillOnce([&backup_name, &expire_time](
                    DatabaseAdminConnection::UpdateBackupParams const& p) {
        EXPECT_EQ(backup_name.FullName(), p.request.backup().name());
        EXPECT_THAT(expire_time,
                    MakeTimestamp(p.request.backup().expire_time()).value());
        gcsa::Backup response;
        response.set_name(p.request.backup().name());
        *response.mutable_expire_time() = p.request.backup().expire_time();
        response.set_state(gcsa::Backup::READY);
        return response;
      })
      .WillOnce([&backup_name, &expire_time](
                    DatabaseAdminConnection::UpdateBackupParams const& p) {
        EXPECT_EQ(backup_name.FullName(), p.request.backup().name());
        EXPECT_THAT(MakeTimestamp(
                        expire_time.get<std::chrono::system_clock::time_point>()
                            .value())
                        .value(),  // loss of precision
                    MakeTimestamp(p.request.backup().expire_time()).value());
        gcsa::Backup response;
        response.set_name(p.request.backup().name());
        *response.mutable_expire_time() = p.request.backup().expire_time();
        response.set_state(gcsa::Backup::READY);
        return response;
      });

  DatabaseAdminClient client(std::move(mock));
  auto response = client.UpdateBackupExpireTime(backup, expire_time);
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(gcsa::Backup::READY, response->state());
  EXPECT_EQ(backup_name.FullName(), response->name());
  EXPECT_THAT(expire_time, MakeTimestamp(response->expire_time()).value());

  // Exercise the old interface with a `time_point` expiration parameter.
  response = client.UpdateBackupExpireTime(
      backup, expire_time.get<std::chrono::system_clock::time_point>().value());
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(gcsa::Backup::READY, response->state());
  EXPECT_EQ(backup_name.FullName(), response->name());
  EXPECT_THAT(
      MakeTimestamp(
          expire_time.get<std::chrono::system_clock::time_point>().value())
          .value(),  // loss of precision
      MakeTimestamp(response->expire_time()).value());
}

TEST(DatabaseAdminClientTest, ListBackupOperations) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  Instance const expected_instance("test-project", "test-instance");
  std::string expected_filter("test-filter");
  EXPECT_CALL(*mock, ListBackupOperations(_))
      .WillOnce(
          [&expected_instance, &expected_filter](
              DatabaseAdminConnection::ListBackupOperationsParams const& p) {
            EXPECT_EQ(expected_instance, p.instance);
            EXPECT_EQ(expected_filter, p.filter);

            return google::cloud::internal::MakePaginationRange<
                ListBackupOperationsRange>(
                gcsa::ListBackupOperationsRequest{},
                [](gcsa::ListBackupOperationsRequest const&) {
                  return StatusOr<gcsa::ListBackupOperationsResponse>(
                      Status(StatusCode::kPermissionDenied, "uh-oh"));
                },
                [](gcsa::ListBackupOperationsResponse const&) {
                  return std::vector<google::longrunning::Operation>{};
                });
          });

  DatabaseAdminClient client(mock);
  auto range = client.ListBackupOperations(expected_instance, expected_filter);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

TEST(DatabaseAdminClientTest, ListDatabaseOperations) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  Instance const expected_instance("test-project", "test-instance");
  std::string expected_filter("test-filter");
  EXPECT_CALL(*mock, ListDatabaseOperations(_))
      .WillOnce(
          [&expected_instance, &expected_filter](
              DatabaseAdminConnection::ListDatabaseOperationsParams const& p) {
            EXPECT_EQ(expected_instance, p.instance);
            EXPECT_EQ(expected_filter, p.filter);

            return google::cloud::internal::MakePaginationRange<
                ListDatabaseOperationsRange>(
                gcsa::ListDatabaseOperationsRequest{},
                [](gcsa::ListDatabaseOperationsRequest const&) {
                  return StatusOr<gcsa::ListDatabaseOperationsResponse>(
                      Status(StatusCode::kPermissionDenied, "uh-oh"));
                },
                [](gcsa::ListDatabaseOperationsResponse const&) {
                  return std::vector<google::longrunning::Operation>{};
                });
          });

  DatabaseAdminClient client(mock);
  auto range =
      client.ListDatabaseOperations(expected_instance, expected_filter);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
