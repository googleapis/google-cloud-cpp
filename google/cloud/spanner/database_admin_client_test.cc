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
using ::testing::ElementsAre;
using ::testing::Invoke;
namespace gcsa = ::google::spanner::admin::database::v1;

/// @test Verify DatabaseAdminClient uses CreateDatabase() correctly.
TEST(DatabaseAdminClientTest, CreateDatabase) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();

  Database dbase("test-project", "test-instance", "test-db");

  EXPECT_CALL(*mock, CreateDatabase(_))
      .WillOnce(Invoke(
          [&dbase](DatabaseAdminConnection::CreateDatabaseParams const& p) {
            EXPECT_EQ(p.database, dbase);
            EXPECT_THAT(p.extra_statements, ElementsAre("-- NOT SQL for test"));
            gcsa::Database database;
            database.set_name(dbase.FullName());
            database.set_state(gcsa::Database::CREATING);
            return make_ready_future(make_status_or(database));
          }));

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
      .WillOnce(
          Invoke([&dbase](DatabaseAdminConnection::GetDatabaseParams const& p) {
            EXPECT_EQ(dbase, p.database);
            gcsa::Database response;
            response.set_name(p.database.FullName());
            response.set_state(gcsa::Database::READY);
            return response;
          }));

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
      .WillOnce(
          Invoke([&expected_name](
                     DatabaseAdminConnection::GetDatabaseDdlParams const& p) {
            EXPECT_EQ(expected_name, p.database);
            gcsa::GetDatabaseDdlResponse response;
            response.add_statements("CREATE DATABASE test-database");
            return response;
          }));

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
      .WillOnce(Invoke(
          [&dbase](DatabaseAdminConnection::UpdateDatabaseParams const& p) {
            EXPECT_EQ(p.database, dbase);
            EXPECT_THAT(p.statements, ElementsAre("-- test only: NOT SQL"));
            gcsa::UpdateDatabaseDdlMetadata metadata;
            metadata.add_statements("-- test only: NOT SQL");
            return make_ready_future(make_status_or(metadata));
          }));

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
      .WillOnce(
          Invoke([&expected_db, &expected_role, &expected_member](
                     DatabaseAdminConnection::GetIamPolicyParams const& p) {
            EXPECT_EQ(expected_db, p.database);
            google::iam::v1::Policy response;
            auto& binding = *response.add_bindings();
            binding.set_role(expected_role);
            *binding.add_members() = expected_member;
            return response;
          }));

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
      .WillOnce(Invoke(
          [&expected_db](DatabaseAdminConnection::SetIamPolicyParams const& p) {
            EXPECT_EQ(expected_db, p.database);
            return p.policy;
          }));
  DatabaseAdminClient client(std::move(mock));
  auto response = client.SetIamPolicy(expected_db, {});
  EXPECT_STATUS_OK(response);
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
