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
#include "google/cloud/bigtable/testing/mock_admin_client.h"
#include "google/cloud/bigtable/testing/mock_instance_admin_client.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include <google/protobuf/util/time_util.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {
namespace {

using ::google::cloud::internal::DefaultPRNG;
using ::testing::HasSubstr;
using ::testing::ReturnRef;

TEST(CleanupStaleResources, CleanupOldTables) {
  using MockAdminClient = ::google::cloud::bigtable::testing::MockAdminClient;
  namespace btadmin = google::bigtable::admin::v2;

  auto const expired_tp =
      std::chrono::system_clock::now() - std::chrono::hours(72);
  auto const active_tp = std::chrono::system_clock::now();
  DefaultPRNG generator;
  auto const id_1 = RandomTableId(generator, expired_tp);
  auto const id_2 = RandomTableId(generator, expired_tp);
  auto const id_3 = RandomTableId(generator, active_tp);
  auto const id_4 = RandomTableId(generator, active_tp);

  std::string const project_id = "test-project-id";
  std::string const instance_id = "test-instance-id";

  auto mock = std::make_shared<MockAdminClient>();
  EXPECT_CALL(*mock, project()).WillRepeatedly(ReturnRef(project_id));

  EXPECT_CALL(*mock, ListTables)
      .WillOnce([&](grpc::ClientContext*,
                    btadmin::ListTablesRequest const& request,
                    btadmin::ListTablesResponse* response) {
        for (auto const& id : {id_1, id_2, id_3, id_4}) {
          auto& t = *response->add_tables();
          t.set_name(request.parent() + "/tables/" + id);
        }
        response->clear_next_page_token();
        return grpc::Status::OK;
      });

  bigtable::TableAdmin admin(mock, instance_id);
  auto const name_1 = admin.TableName(id_1);
  auto const name_2 = admin.TableName(id_2);
  // Verify only `name_1` and `name_2` are deleted.
  EXPECT_CALL(*mock, DeleteTable)
      .WillOnce([&](grpc::ClientContext*,
                    btadmin::DeleteTableRequest const& request,
                    google::protobuf::Empty*) {
        EXPECT_EQ(request.name(), name_1);
        return grpc::Status::OK;
      })
      .WillOnce([&](grpc::ClientContext*,
                    btadmin::DeleteTableRequest const& request,
                    google::protobuf::Empty*) {
        EXPECT_EQ(request.name(), name_2);
        return grpc::Status::OK;
      });

  CleanupStaleTables(admin);
}

TEST(CleanupStaleResources, CleanupStaleBackups) {
  using MockAdminClient = ::google::cloud::bigtable::testing::MockAdminClient;
  using google::protobuf::util::TimeUtil;
  namespace btadmin = google::bigtable::admin::v2;

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

  std::string const prefix = "project/" + project_id + "/instances/" +
                             instance_id + "/clusters/" + cluster_id +
                             "/backups/";
  google::bigtable::admin::v2::Backup backup_2;
  backup_2.set_name(prefix + id_2);
  *backup_2.mutable_expire_time() =
      TimeUtil::GetCurrentTime() - TimeUtil::HoursToDuration(24 * 8);

  auto mock = std::make_shared<MockAdminClient>();
  EXPECT_CALL(*mock, project()).WillRepeatedly(ReturnRef(project_id));

  EXPECT_CALL(*mock, ListBackups)
      .WillOnce([&](grpc::ClientContext*, btadmin::ListBackupsRequest const&,
                    btadmin::ListBackupsResponse* response) {
        for (auto const& backup : {id_1, id_2, id_3, id_4, id_5}) {
          auto& b = *response->add_backups();
          google::bigtable::admin::v2::Backup backup_1;
          b.set_name(prefix + backup);
        }
        response->clear_next_page_token();
        return grpc::Status::OK;
      });

  bigtable::TableAdmin admin(mock, instance_id);

  EXPECT_CALL(*mock, DeleteBackup)
      .WillOnce([&](grpc::ClientContext*,
                    btadmin::DeleteBackupRequest const& request,
                    google::protobuf::Empty*) {
        EXPECT_THAT(request.name(), HasSubstr(id_1));
        return grpc::Status::OK;
      })
      .WillOnce([&](grpc::ClientContext*,
                    btadmin::DeleteBackupRequest const& request,
                    google::protobuf::Empty*) {
        EXPECT_THAT(request.name(), HasSubstr(id_2));
        return grpc::Status::OK;
      })
      .WillOnce([&](grpc::ClientContext*,
                    btadmin::DeleteBackupRequest const& request,
                    google::protobuf::Empty*) {
        EXPECT_THAT(request.name(), HasSubstr(id_5));
        return grpc::Status::OK;
      });

  CleanupStaleBackups(admin);
}

TEST(CleanupStaleResources, CleanupOldInstances) {
  using MockAdminClient =
      ::google::cloud::bigtable::testing::MockInstanceAdminClient;
  namespace btadmin = google::bigtable::admin::v2;

  auto const expired_tp =
      std::chrono::system_clock::now() - std::chrono::hours(72);
  DefaultPRNG generator;
  auto const active_tp = std::chrono::system_clock::now();
  auto const id_1 = RandomInstanceId(generator, expired_tp);
  auto const id_2 = RandomInstanceId(generator, expired_tp);
  auto const id_3 = RandomInstanceId(generator, active_tp);
  auto const id_4 = RandomInstanceId(generator, active_tp);

  std::string const project_id = "test-project-id";

  auto mock = std::make_shared<MockAdminClient>();
  EXPECT_CALL(*mock, project()).WillRepeatedly(ReturnRef(project_id));

  EXPECT_CALL(*mock, ListInstances)
      .WillOnce([&](grpc::ClientContext*,
                    btadmin::ListInstancesRequest const& request,
                    btadmin::ListInstancesResponse* response) {
        for (auto const& id : {id_1, id_2, id_3, id_4}) {
          auto& instance = *response->add_instances();
          instance.set_name(request.parent() + "/instances/" + id);
        }
        response->clear_next_page_token();
        return grpc::Status::OK;
      });

  bigtable::InstanceAdmin admin(mock);
  auto const name_1 = admin.InstanceName(id_1);
  auto const name_2 = admin.InstanceName(id_2);

  // Verify only `name_1` and `name_2` are deleted.
  EXPECT_CALL(*mock, DeleteInstance)
      .WillOnce([&](grpc::ClientContext*,
                    btadmin::DeleteInstanceRequest const& request,
                    google::protobuf::Empty*) {
        EXPECT_EQ(request.name(), name_1);
        return grpc::Status::OK;
      })
      .WillOnce([&](grpc::ClientContext*,
                    btadmin::DeleteInstanceRequest const& request,
                    google::protobuf::Empty*) {
        EXPECT_EQ(request.name(), name_2);
        return grpc::Status::OK;
      });

  CleanupStaleInstances(admin);
}

}  // namespace
}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
