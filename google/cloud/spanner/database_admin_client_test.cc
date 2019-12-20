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
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::google::cloud::spanner_mocks::MockDatabaseAdminConnection;
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
  EXPECT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
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
  EXPECT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
  auto db = fut.get();
  EXPECT_STATUS_OK(db);

  EXPECT_THAT(db->statements(), ElementsAre("-- test only: NOT SQL"));
}

TEST(DatabaseAdminClientTest, ListInstances) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  Instance const expected_instance("test-project", "test-instance");
  EXPECT_CALL(*mock, ListDatabases(_))
      .WillOnce([&expected_instance](
                    DatabaseAdminConnection::ListDatabasesParams const& p) {
        EXPECT_EQ(expected_instance, p.instance);

        return ListDatabaseRange(
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
  EXPECT_EQ(StatusCode::kPermissionDenied, begin->status().code());
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
    return optional<google::iam::v1::Policy>{};
  });
  EXPECT_EQ(StatusCode::kPermissionDenied, actual.status().code());
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
    return optional<google::iam::v1::Policy>{};
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
  EXPECT_EQ(StatusCode::kAborted, actual.status().code());
  EXPECT_THAT(actual.status().message(), HasSubstr("test-msg"));
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

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
