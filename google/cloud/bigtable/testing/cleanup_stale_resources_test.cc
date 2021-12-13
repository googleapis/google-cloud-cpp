// Copyright 2021 Google LLC
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

#include "google/cloud/bigtable/testing/cleanup_stale_resources.h"
#include "google/cloud/bigtable/admin/mocks/mock_bigtable_instance_admin_connection.h"
#include "google/cloud/bigtable/admin/mocks/mock_bigtable_table_admin_connection.h"
#include "google/cloud/bigtable/resource_names.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/project.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {
namespace {

using ::google::cloud::internal::DefaultPRNG;
using ::google::cloud::internal::MakeStreamRange;
using ::google::cloud::internal::StreamReader;
using ::testing::HasSubstr;

TEST(CleanupStaleResources, CleanupOldTables) {
  using MockConnection =
      ::google::cloud::bigtable_admin_mocks::MockBigtableTableAdminConnection;
  namespace btadmin = ::google::bigtable::admin::v2;

  std::string const project_id = "test-project-id";
  std::string const instance_id = "test-instance-id";
  auto const expired_tp =
      std::chrono::system_clock::now() - std::chrono::hours(72);
  auto const active_tp = std::chrono::system_clock::now();
  DefaultPRNG generator;
  auto const id_1 = RandomTableId(generator, expired_tp);
  auto const id_2 = RandomTableId(generator, expired_tp);
  auto const id_3 = RandomTableId(generator, active_tp);
  auto const id_4 = RandomTableId(generator, active_tp);
  auto const instance_name = InstanceName(project_id, instance_id);
  auto const name_1 = TableName(project_id, instance_id, id_1);
  auto const name_2 = TableName(project_id, instance_id, id_2);
  std::vector<std::string> const ids = {id_1, id_2, id_3, id_4};
  auto iter = ids.begin();

  auto mock = std::make_shared<MockConnection>();

  EXPECT_CALL(*mock, ListTables)
      .WillOnce([&](btadmin::ListTablesRequest const& request) {
        EXPECT_EQ(request.parent(), instance_name);
        EXPECT_EQ(request.view(), btadmin::Table::NAME_ONLY);

        auto reader = [&]() -> StreamReader<btadmin::Table>::result_type {
          if (iter != ids.end()) {
            btadmin::Table t;
            t.set_name(instance_name + "/tables/" + *iter);
            ++iter;
            return t;
          }
          return Status();
        };
        return MakeStreamRange<btadmin::Table>(std::move(reader));
      });

  // Verify only `name_1` and `name_2` are deleted.
  EXPECT_CALL(*mock, DeleteTable)
      .WillOnce([&](btadmin::DeleteTableRequest const& request) {
        EXPECT_EQ(request.name(), name_1);
        return Status();
      })
      .WillOnce([&](btadmin::DeleteTableRequest const& request) {
        EXPECT_EQ(request.name(), name_2);
        return Status();
      });

  CleanupStaleTables(mock, project_id, instance_id);
}

TEST(CleanupStaleResources, CleanupStaleBackups) {
  using MockConnection =
      ::google::cloud::bigtable_admin_mocks::MockBigtableTableAdminConnection;
  namespace btadmin = ::google::bigtable::admin::v2;

  std::string const project_id = "test-project-id";
  std::string const instance_id = "test-instance-id";
  std::string const cluster_id = "test-instance-id-c1";
  auto const expired_tp =
      std::chrono::system_clock::now() - std::chrono::hours(72);
  auto const active_tp = std::chrono::system_clock::now();
  DefaultPRNG generator;
  auto const id_1 = RandomBackupId(generator, expired_tp);
  auto const id_2 = RandomBackupId(generator, expired_tp);
  auto const id_3 = RandomBackupId(generator, active_tp);
  auto const id_4 = RandomBackupId(generator, active_tp);
  auto const id_5 = RandomBackupId(generator, expired_tp);
  auto const cluster_name = ClusterName(project_id, instance_id, cluster_id);
  std::vector<std::string> const ids = {id_1, id_2, id_3, id_4, id_5};
  auto iter = ids.begin();

  auto mock = std::make_shared<MockConnection>();

  EXPECT_CALL(*mock, ListBackups)
      .WillOnce([&](btadmin::ListBackupsRequest const& request) {
        EXPECT_EQ(request.parent(), ClusterName(project_id, instance_id, "-"));

        auto reader = [&]() -> StreamReader<btadmin::Backup>::result_type {
          if (iter != ids.end()) {
            btadmin::Backup b;
            b.set_name(cluster_name + "/tables/" + *iter);
            ++iter;
            return b;
          }
          return Status();
        };
        return MakeStreamRange<btadmin::Backup>(std::move(reader));
      });

  EXPECT_CALL(*mock, DeleteBackup)
      .WillOnce([&](btadmin::DeleteBackupRequest const& request) {
        EXPECT_THAT(request.name(), HasSubstr(id_1));
        return Status();
      })
      .WillOnce([&](btadmin::DeleteBackupRequest const& request) {
        EXPECT_THAT(request.name(), HasSubstr(id_2));
        return Status();
      })
      .WillOnce([&](btadmin::DeleteBackupRequest const& request) {
        EXPECT_THAT(request.name(), HasSubstr(id_5));
        return Status();
      });

  CleanupStaleBackups(mock, project_id, instance_id);
}

TEST(CleanupStaleResources, CleanupOldInstances) {
  using MockConnection = ::google::cloud::bigtable_admin_mocks::
      MockBigtableInstanceAdminConnection;
  namespace btadmin = ::google::bigtable::admin::v2;

  std::string const project_id = "test-project-id";
  auto const expired_tp =
      std::chrono::system_clock::now() - std::chrono::hours(72);
  DefaultPRNG generator;
  auto const active_tp = std::chrono::system_clock::now();
  auto const id_1 = RandomInstanceId(generator, expired_tp);
  auto const id_2 = RandomInstanceId(generator, expired_tp);
  auto const id_3 = RandomInstanceId(generator, active_tp);
  auto const id_4 = RandomInstanceId(generator, active_tp);
  auto const name_1 = InstanceName(project_id, id_1);
  auto const name_2 = InstanceName(project_id, id_2);

  auto mock = std::make_shared<MockConnection>();

  EXPECT_CALL(*mock, ListInstances)
      .WillOnce([&](btadmin::ListInstancesRequest const& request) {
        EXPECT_EQ(request.parent(), Project(project_id).FullName());
        btadmin::ListInstancesResponse response;
        for (auto const& id : {id_1, id_2, id_3, id_4}) {
          auto& instance = *response.add_instances();
          instance.set_name(request.parent() + "/instances/" + id);
        }
        return make_status_or(response);
      });

  // Verify only `name_1` and `name_2` are deleted.
  EXPECT_CALL(*mock, DeleteInstance)
      .WillOnce([&](btadmin::DeleteInstanceRequest const& request) {
        EXPECT_EQ(request.name(), name_1);
        return Status();
      })
      .WillOnce([&](btadmin::DeleteInstanceRequest const& request) {
        EXPECT_EQ(request.name(), name_2);
        return Status();
      });

  CleanupStaleInstances(mock, project_id);
}

}  // namespace
}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
