// Copyright 2020 Google LLC
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

#include "google/cloud/internal/pagination_range.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "generator/integration_tests/golden/golden_thing_admin_client.gcpcxx.pb.h"
#include "generator/integration_tests/golden/mocks/mock_golden_thing_admin_connection.gcpcxx.pb.h"
#include <google/iam/v1/policy.pb.h>
#include <google/protobuf/util/field_mask_util.h>
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden {
inline namespace GOOGLE_CLOUD_CPP_GENERATED_NS {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;

TEST(GoldenThingAdminClientTest, CopyMoveEquality) {
  auto conn1 = std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();
  auto conn2 = std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();

  GoldenThingAdminClient c1(conn1);
  GoldenThingAdminClient c2(conn2);
  EXPECT_NE(c1, c2);

  // Copy construction
  GoldenThingAdminClient c3 = c1;
  EXPECT_EQ(c3, c1);
  EXPECT_NE(c3, c2);

  // Copy assignment
  c3 = c2;
  EXPECT_EQ(c3, c2);

  // Move construction
  GoldenThingAdminClient c4 = std::move(c3);
  EXPECT_EQ(c4, c2);

  // Move assignment
  c1 = std::move(c4);
  EXPECT_EQ(c1, c2);
}

TEST(GoldenThingAdminClientTest, ListDatabases) {
  auto mock = std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();
  std::string expected_instance =
      "/projects/test-project/instances/test-instance";
  EXPECT_CALL(*mock, ListDatabases)
      .Times(2)
      .WillRepeatedly([expected_instance](::google::test::admin::database::v1::
                                              ListDatabasesRequest const& r) {
        EXPECT_EQ(expected_instance, r.parent());

        return google::cloud::internal::MakePaginationRange<
            StreamRange<::google::test::admin::database::v1::Database>>(
            ::google::test::admin::database::v1::ListDatabasesRequest{},
            [](::google::test::admin::database::v1::
                   ListDatabasesRequest const&) {
              return StatusOr<
                  ::google::test::admin::database::v1::ListDatabasesResponse>(
                  Status(StatusCode::kPermissionDenied, "uh-oh"));
            },
            [](::google::test::admin::database::v1::
                   ListDatabasesResponse const&) {
              return std::vector<
                  ::google::test::admin::database::v1::Database>{};
            });
      });

  GoldenThingAdminClient client(mock);
  auto range = client.ListDatabases(expected_instance);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
  ::google::test::admin::database::v1::ListDatabasesRequest request;
  request.set_parent(expected_instance);
  range = client.ListDatabases(request);
  begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminClientTest, CreateDatabase) {
  auto mock = std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();
  std::string expected_instance =
      "/projects/test-project/instances/test-instance";
  std::string expected_database =
      "/projects/test-project/instances/test-instance/databases/test-db";
  EXPECT_CALL(*mock, CreateDatabase)
      .Times(2)
      .WillRepeatedly(
          [expected_instance](
              ::google::test::admin::database::v1::CreateDatabaseRequest const&
                  r) {
            EXPECT_EQ(expected_instance, r.parent());
            EXPECT_THAT(r.create_statement(),
                        HasSubstr("create database test-db"));
            ::google::test::admin::database::v1::Database database;
            database.set_name(r.parent() + "/databases/test-db");
            database.set_state(
                ::google::test::admin::database::v1::Database::CREATING);
            return make_ready_future(make_status_or(database));
          });
  GoldenThingAdminClient client(std::move(mock));
  auto fut =
      client.CreateDatabase(expected_instance, "create database test-db");
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
  auto db = fut.get();
  EXPECT_STATUS_OK(db);
  EXPECT_EQ(expected_database, db->name());
  EXPECT_EQ(::google::test::admin::database::v1::Database::CREATING,
            db->state());
  ::google::test::admin::database::v1::CreateDatabaseRequest request;
  request.set_parent(expected_instance);
  request.set_create_statement("create database test-db");
  fut = client.CreateDatabase(request);
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
  db = fut.get();
  EXPECT_STATUS_OK(db);
  EXPECT_EQ(expected_database, db->name());
  EXPECT_EQ(::google::test::admin::database::v1::Database::CREATING,
            db->state());
}

TEST(GoldenThingAdminClientTest, GetDatabase) {
  auto mock = std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();
  std::string expected_database =
      "/projects/test-project/instances/test-instance/databases/test-db";
  EXPECT_CALL(*mock, GetDatabase)
      .Times(2)
      .WillRepeatedly(
          [expected_database](
              ::google::test::admin::database::v1::GetDatabaseRequest const&
                  request) {
            EXPECT_EQ(expected_database, request.name());
            ::google::test::admin::database::v1::Database response;
            response.set_name(request.name());
            return response;
          });
  GoldenThingAdminClient client(std::move(mock));
  auto response = client.GetDatabase(expected_database);
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(response->name(), expected_database);
  ::google::test::admin::database::v1::GetDatabaseRequest request;
  request.set_name(expected_database);
  response = client.GetDatabase(request);
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(response->name(), expected_database);
}

TEST(GoldenThingAdminClientTest, UpdateDatabase) {
  auto mock = std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();
  std::string expected_database =
      "/projects/test-project/instances/test-instance/databases/test-db";
  EXPECT_CALL(*mock, UpdateDatabaseDdl)
      .Times(2)
      .WillRepeatedly([expected_database](
                          ::google::test::admin::database::v1::
                              UpdateDatabaseDdlRequest const& r) {
        EXPECT_EQ(expected_database, r.database());
        EXPECT_THAT(r.statements(), ElementsAre("-- test only: NOT SQL"));
        ::google::test::admin::database::v1::UpdateDatabaseDdlMetadata metadata;
        metadata.add_statements("-- test only: NOT SQL");
        return make_ready_future(make_status_or(metadata));
      });
  GoldenThingAdminClient client(std::move(mock));
  auto fut =
      client.UpdateDatabaseDdl(expected_database, {"-- test only: NOT SQL"});
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
  auto db = fut.get();
  EXPECT_STATUS_OK(db);
  EXPECT_THAT(db->statements(), ElementsAre("-- test only: NOT SQL"));
  ::google::test::admin::database::v1::UpdateDatabaseDdlRequest request;
  request.set_database(expected_database);
  request.add_statements("-- test only: NOT SQL");
  fut = client.UpdateDatabaseDdl(request);
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
  db = fut.get();
  EXPECT_STATUS_OK(db);
  EXPECT_THAT(db->statements(), ElementsAre("-- test only: NOT SQL"));
}

TEST(GoldenThingAdminClientTest, DropDatabase) {
  auto mock = std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();
  std::string expected_database =
      "/projects/test-project/instances/test-instance/databases/test-db";
  EXPECT_CALL(*mock, DropDatabase)
      .Times(2)
      .WillRepeatedly(
          [expected_database](
              ::google::test::admin::database::v1::DropDatabaseRequest const&
                  request) {
            EXPECT_EQ(expected_database, request.database());
            return Status();
          });
  GoldenThingAdminClient client(std::move(mock));
  auto response = client.DropDatabase(expected_database);
  EXPECT_STATUS_OK(response);
  ::google::test::admin::database::v1::DropDatabaseRequest request;
  request.set_database(expected_database);
  response = client.DropDatabase(request);
  EXPECT_STATUS_OK(response);
}

TEST(GoldenThingAdminClientTest, GetDatabaseDdl) {
  auto mock = std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();
  std::string expected_database =
      "/projects/test-project/instances/test-instance/databases/test-db";
  EXPECT_CALL(*mock, GetDatabaseDdl)
      .Times(2)
      .WillRepeatedly([expected_database](::google::test::admin::database::v1::
                                              GetDatabaseDdlRequest const& r) {
        EXPECT_EQ(expected_database, r.database());
        ::google::test::admin::database::v1::GetDatabaseDdlResponse response;
        response.add_statements("CREATE DATABASE test-db");
        return response;
      });
  GoldenThingAdminClient client(std::move(mock));
  auto response = client.GetDatabaseDdl(expected_database);
  EXPECT_STATUS_OK(response);
  ASSERT_EQ(1, response->statements_size());
  ASSERT_EQ("CREATE DATABASE test-db", response->statements(0));
  ::google::test::admin::database::v1::GetDatabaseDdlRequest request;
  request.set_database(expected_database);
  response = client.GetDatabaseDdl(request);
  EXPECT_STATUS_OK(response);
  ASSERT_EQ(1, response->statements_size());
  ASSERT_EQ("CREATE DATABASE test-db", response->statements(0));
}

TEST(GoldenThingAdminClientTest, SetIamPolicy) {
  auto mock = std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();
  std::string expected_database =
      "/projects/test-project/instances/test-instance/databases/test-db";
  EXPECT_CALL(*mock, SetIamPolicy)
      .Times(2)
      .WillRepeatedly(
          [expected_database](::google::iam::v1::SetIamPolicyRequest const& r) {
            EXPECT_EQ(expected_database, r.resource());
            return r.policy();
          });
  GoldenThingAdminClient client(std::move(mock));
  auto response =
      client.SetIamPolicy(expected_database, google::iam::v1::Policy{});
  EXPECT_STATUS_OK(response);
  ::google::iam::v1::SetIamPolicyRequest request;
  request.set_resource(expected_database);
  *request.mutable_policy() = google::iam::v1::Policy{};
  response = client.SetIamPolicy(request);
  EXPECT_STATUS_OK(response);
}

TEST(GoldenThingAdminClientTest, GetIamPolicy) {
  auto mock = std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();
  std::string expected_database =
      "/projects/test-project/instances/test-instance/databases/test-db";
  std::string const expected_role = "roles/spanner.databaseReader";
  std::string const expected_member = "user:foobar@example.com";
  EXPECT_CALL(*mock, GetIamPolicy)
      .Times(2)
      .WillRepeatedly([expected_database, expected_role, expected_member](
                          ::google::iam::v1::GetIamPolicyRequest const& r) {
        EXPECT_EQ(expected_database, r.resource());
        google::iam::v1::Policy response;
        auto& binding = *response.add_bindings();
        binding.set_role(expected_role);
        *binding.add_members() = expected_member;
        return response;
      });
  GoldenThingAdminClient client(std::move(mock));
  auto response = client.GetIamPolicy(expected_database);
  EXPECT_STATUS_OK(response);
  ASSERT_EQ(1, response->bindings().size());
  ASSERT_EQ(expected_role, response->bindings().Get(0).role());
  ASSERT_EQ(1, response->bindings().Get(0).members().size());
  ASSERT_EQ(expected_member, response->bindings().Get(0).members().Get(0));
  ::google::iam::v1::GetIamPolicyRequest request;
  request.set_resource(expected_database);
  response = client.GetIamPolicy(request);
  EXPECT_STATUS_OK(response);
  ASSERT_EQ(1, response->bindings().size());
  ASSERT_EQ(expected_role, response->bindings().Get(0).role());
  ASSERT_EQ(1, response->bindings().Get(0).members().size());
  ASSERT_EQ(expected_member, response->bindings().Get(0).members().Get(0));
}

TEST(GoldenThingAdminClientTest, TestIamPermissions) {
  auto mock = std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();
  std::string expected_database =
      "/projects/test-project/instances/test-instance/databases/test-db";
  std::string expected_permission = "spanner.databases.read";
  EXPECT_CALL(*mock, TestIamPermissions)
      .Times(2)
      .WillRepeatedly(
          [expected_database, expected_permission](
              ::google::iam::v1::TestIamPermissionsRequest const& r) {
            EXPECT_EQ(expected_database, r.resource());
            EXPECT_EQ(1, r.permissions().size());
            EXPECT_EQ(expected_permission, r.permissions(0));
            google::iam::v1::TestIamPermissionsResponse response;
            response.add_permissions(expected_permission);
            return response;
          });
  GoldenThingAdminClient client(std::move(mock));
  auto response =
      client.TestIamPermissions(expected_database, {expected_permission});
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(1, response->permissions_size());
  EXPECT_EQ(expected_permission, response->permissions(0));
  ::google::iam::v1::TestIamPermissionsRequest request;
  request.set_resource(expected_database);
  request.add_permissions(expected_permission);
  response = client.TestIamPermissions(request);
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(1, response->permissions_size());
  EXPECT_EQ(expected_permission, response->permissions(0));
}

TEST(GoldenThingAdminClientTest, CreateBackup) {
  auto mock = std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();
  std::string expected_instance =
      "/projects/test-project/instances/test-instance";
  std::string expected_database =
      "/projects/test-project/instances/test-instance/databases/test-db";
  std::string backup_id = "test-backup";
  std::string expected_backup_name =
      "/projects/test-project/instances/test-instance/backups/test-backup";
  std::chrono::system_clock::time_point expire_time =
      std::chrono::system_clock::now() + std::chrono::hours(7);
  EXPECT_CALL(*mock, CreateBackup)
      .Times(2)
      .WillRepeatedly(
          [expected_database, expire_time, backup_id, expected_backup_name](
              ::google::test::admin::database::v1::CreateBackupRequest const&
                  r) {
            EXPECT_EQ(expected_database, r.backup().database());
            EXPECT_THAT(google::cloud::internal::ToProtoTimestamp(expire_time),
                        IsProtoEqual(r.backup().expire_time()));
            ::google::test::admin::database::v1::Backup backup;
            backup.set_name(expected_backup_name);
            backup.set_state(
                ::google::test::admin::database::v1::Backup::CREATING);
            return make_ready_future(make_status_or(backup));
          });
  GoldenThingAdminClient client(std::move(mock));
  ::google::test::admin::database::v1::Backup backup;
  backup.set_database(expected_database);
  *backup.mutable_expire_time() =
      google::cloud::internal::ToProtoTimestamp(expire_time);
  auto fut = client.CreateBackup(expected_instance, backup, backup_id);
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
  auto response = fut.get();
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(expected_backup_name, response->name());
  EXPECT_EQ(::google::test::admin::database::v1::Backup::CREATING,
            response->state());
  ::google::test::admin::database::v1::CreateBackupRequest request;
  request.set_parent(expected_instance);
  request.set_backup_id(backup_id);
  *request.mutable_backup() = backup;
  fut = client.CreateBackup(request);
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
  response = fut.get();
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(expected_backup_name, response->name());
  EXPECT_EQ(::google::test::admin::database::v1::Backup::CREATING,
            response->state());
}

TEST(GoldenThingAdminClientTest, GetBackup) {
  auto mock = std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();
  std::string expected_backup_name =
      "/projects/test-project/instances/test-instance/backups/test-backup";
  EXPECT_CALL(*mock, GetBackup)
      .Times(2)
      .WillRepeatedly(
          [expected_backup_name](
              ::google::test::admin::database::v1::GetBackupRequest const& r) {
            EXPECT_EQ(expected_backup_name, r.name());
            ::google::test::admin::database::v1::Backup response;
            response.set_name(r.name());
            response.set_state(
                ::google::test::admin::database::v1::Backup::READY);
            return response;
          });
  GoldenThingAdminClient client(std::move(mock));
  auto response = client.GetBackup(expected_backup_name);
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(::google::test::admin::database::v1::Backup::READY,
            response->state());
  EXPECT_EQ(expected_backup_name, response->name());
  ::google::test::admin::database::v1::GetBackupRequest request;
  request.set_name(expected_backup_name);
  response = client.GetBackup(request);
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(::google::test::admin::database::v1::Backup::READY,
            response->state());
  EXPECT_EQ(expected_backup_name, response->name());
}

TEST(GoldenThingAdminClientTest, UpdateBackupExpireTime) {
  auto mock = std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();
  std::string expected_backup_name =
      "/projects/test-project/instances/test-instance/backups/test-backup";
  std::chrono::system_clock::time_point expire_time =
      std::chrono::system_clock::now() + std::chrono::hours(7);
  auto proto_expire_time =
      google::cloud::internal::ToProtoTimestamp(expire_time);
  EXPECT_CALL(*mock, UpdateBackup)
      .Times(2)
      .WillRepeatedly(
          [expected_backup_name, proto_expire_time](
              ::google::test::admin::database::v1::UpdateBackupRequest const&
                  r) {
            EXPECT_EQ(expected_backup_name, r.backup().name());
            EXPECT_THAT(proto_expire_time,
                        IsProtoEqual(r.backup().expire_time()));
            EXPECT_TRUE(
                ::google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(
                    "expire_time", r.update_mask()));
            ::google::test::admin::database::v1::Backup response;
            response.set_name(r.backup().name());
            *response.mutable_expire_time() = r.backup().expire_time();
            response.set_state(
                ::google::test::admin::database::v1::Backup::READY);
            return response;
          });
  ::google::test::admin::database::v1::Backup backup;
  backup.set_name(expected_backup_name);
  *backup.mutable_expire_time() = proto_expire_time;
  google::protobuf::FieldMask update_mask;
  update_mask.add_paths("expire_time");
  GoldenThingAdminClient client(std::move(mock));
  auto response = client.UpdateBackup(backup, update_mask);
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(::google::test::admin::database::v1::Backup::READY,
            response->state());
  EXPECT_EQ(backup.name(), response->name());
  EXPECT_THAT(proto_expire_time, IsProtoEqual(response->expire_time()));
  ::google::test::admin::database::v1::UpdateBackupRequest request;
  *request.mutable_backup() = backup;
  *request.mutable_update_mask() = update_mask;
  response = client.UpdateBackup(request);
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(::google::test::admin::database::v1::Backup::READY,
            response->state());
  EXPECT_EQ(backup.name(), response->name());
  EXPECT_THAT(proto_expire_time, IsProtoEqual(response->expire_time()));
}

TEST(GoldenThingAdminClientTest, DeleteBackup) {
  auto mock = std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();
  std::string expected_backup_name =
      "/projects/test-project/instances/test-instance/backups/test-backup";
  EXPECT_CALL(*mock, DeleteBackup)
      .Times(2)
      .WillRepeatedly(
          [expected_backup_name](
              ::google::test::admin::database::v1::DeleteBackupRequest const&
                  r) {
            EXPECT_EQ(expected_backup_name, r.name());
            return Status();
          });
  GoldenThingAdminClient client(std::move(mock));
  auto response = client.DeleteBackup(expected_backup_name);
  EXPECT_STATUS_OK(response);
  ::google::test::admin::database::v1::DeleteBackupRequest request;
  request.set_name(expected_backup_name);
  response = client.DeleteBackup(request);
  EXPECT_STATUS_OK(response);
}

TEST(GoldenThingAdminClientTest, ListBackups) {
  auto mock = std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();
  std::string expected_instance =
      "/projects/test-project/instances/test-instance";
  EXPECT_CALL(*mock, ListBackups)
      .Times(2)
      .WillRepeatedly([expected_instance](::google::test::admin::database::v1::
                                              ListBackupsRequest const& r) {
        EXPECT_EQ(expected_instance, r.parent());
        return google::cloud::internal::MakePaginationRange<
            StreamRange<::google::test::admin::database::v1::Backup>>(
            ::google::test::admin::database::v1::ListBackupsRequest{},
            [](::google::test::admin::database::v1::ListBackupsRequest const&) {
              return StatusOr<
                  ::google::test::admin::database::v1::ListBackupsResponse>(
                  Status(StatusCode::kPermissionDenied, "uh-oh"));
            },
            [](::google::test::admin::database::v1::
                   ListBackupsResponse const&) {
              return std::vector<::google::test::admin::database::v1::Backup>{};
            });
      });
  GoldenThingAdminClient client(mock);
  auto range = client.ListBackups(expected_instance);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
  ::google::test::admin::database::v1::ListBackupsRequest request;
  request.set_parent(expected_instance);
  range = client.ListBackups(request);
  begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminClientTest, RestoreDatabase) {
  auto mock = std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();
  std::string expected_instance =
      "/projects/test-project/instances/test-instance";
  std::string expected_database =
      "/projects/test-project/instances/test-instance/databases/test-db";
  std::string expected_backup_name =
      "/projects/test-project/instances/test-instance/backups/test-backup";
  EXPECT_CALL(*mock, RestoreDatabase)
      .Times(2)
      .WillRepeatedly([expected_backup_name, expected_database,
                       expected_instance](::google::test::admin::database::v1::
                                              RestoreDatabaseRequest const& r) {
        EXPECT_EQ(expected_instance, r.parent());
        EXPECT_EQ(expected_database, r.database_id());
        EXPECT_EQ(expected_backup_name, r.backup());
        ::google::test::admin::database::v1::Database database;
        database.set_name(expected_database);
        database.set_state(
            ::google::test::admin::database::v1::Database::READY_OPTIMIZING);
        return make_ready_future(make_status_or(database));
      });
  GoldenThingAdminClient client(std::move(mock));
  auto fut = client.RestoreDatabase(expected_instance, expected_database,
                                    expected_backup_name);
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
  auto database = fut.get();
  EXPECT_STATUS_OK(database);
  EXPECT_EQ(expected_database, database->name());
  EXPECT_EQ(::google::test::admin::database::v1::Database::READY_OPTIMIZING,
            database->state());
  ::google::test::admin::database::v1::RestoreDatabaseRequest request;
  request.set_parent(expected_instance);
  request.set_database_id(expected_database);
  request.set_backup(expected_backup_name);
  fut = client.RestoreDatabase(request);
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
  database = fut.get();
  EXPECT_STATUS_OK(database);
  EXPECT_EQ(expected_database, database->name());
  EXPECT_EQ(::google::test::admin::database::v1::Database::READY_OPTIMIZING,
            database->state());
}

TEST(GoldenThingAdminClientTest, ListDatabaseOperations) {
  auto mock = std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();
  std::string expected_instance =
      "/projects/test-project/instances/test-instance";
  EXPECT_CALL(*mock, ListDatabaseOperations)
      .Times(2)
      .WillRepeatedly(
          [expected_instance](::google::test::admin::database::v1::
                                  ListDatabaseOperationsRequest const& r) {
            EXPECT_EQ(expected_instance, r.parent());
            return google::cloud::internal::MakePaginationRange<
                StreamRange<::google::longrunning::Operation>>(
                ::google::test::admin::database::v1::
                    ListDatabaseOperationsRequest{},
                [](::google::test::admin::database::v1::
                       ListDatabaseOperationsRequest const&) {
                  return StatusOr<::google::test::admin::database::v1::
                                      ListDatabaseOperationsResponse>(
                      Status(StatusCode::kPermissionDenied, "uh-oh"));
                },
                [](::google::test::admin::database::v1::
                       ListDatabaseOperationsResponse const&) {
                  return std::vector<google::longrunning::Operation>{};
                });
          });
  GoldenThingAdminClient client(mock);
  auto range = client.ListDatabaseOperations(expected_instance);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
  ::google::test::admin::database::v1::ListDatabaseOperationsRequest request;
  request.set_parent(expected_instance);
  range = client.ListDatabaseOperations(request);
  begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminClientTest, ListBackupOperations) {
  auto mock = std::make_shared<golden_mocks::MockGoldenThingAdminConnection>();
  std::string expected_instance =
      "/projects/test-project/instances/test-instance";
  EXPECT_CALL(*mock, ListBackupOperations)
      .Times(2)
      .WillRepeatedly([expected_instance](
                          ::google::test::admin::database::v1::
                              ListBackupOperationsRequest const& r) {
        EXPECT_EQ(expected_instance, r.parent());
        return google::cloud::internal::MakePaginationRange<
            StreamRange<::google::longrunning::Operation>>(
            ::google::test::admin::database::v1::ListBackupOperationsRequest{},
            [](::google::test::admin::database::v1::
                   ListBackupOperationsRequest const&) {
              return StatusOr<::google::test::admin::database::v1::
                                  ListBackupOperationsResponse>(
                  Status(StatusCode::kPermissionDenied, "uh-oh"));
            },
            [](::google::test::admin::database::v1::
                   ListBackupOperationsResponse const&) {
              return std::vector<google::longrunning::Operation>{};
            });
      });
  GoldenThingAdminClient client(mock);
  auto range = client.ListBackupOperations(expected_instance);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
  ::google::test::admin::database::v1::ListBackupOperationsRequest request;
  request.set_parent(expected_instance);
  range = client.ListBackupOperations(request);
  begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace golden
}  // namespace cloud
}  // namespace google
