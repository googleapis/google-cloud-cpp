// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/internal/getenv.h"
#include <google/protobuf/util/time_util.h>
#include <sstream>

namespace {

using google::cloud::bigtable::examples::Usage;

void CreateBackup(google::cloud::bigtable::TableAdmin admin,
                  std::vector<std::string> const& argv) {
  //! [create backup]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id, std::string cluster_id,
     std::string backup_id, std::string expire_time_string) {
    google::protobuf::Timestamp expire_time;
    if (!google::protobuf::util::TimeUtil::FromString(expire_time_string,
                                                      &expire_time)) {
      throw std::runtime_error("Unable to parse expire_time:" +
                               expire_time_string);
    }
    StatusOr<google::bigtable::admin::v2::Backup> backup =
        admin.CreateBackup(cbt::TableAdmin::CreateBackupParams(
            std::move(cluster_id), std::move(backup_id), std::move(table_id),
            std::move(expire_time)));
    if (!backup) throw std::runtime_error(backup.status().message());
    std::cout << "Backup successfully created: " << backup->DebugString()
              << "\n";
  }
  //! [create backup]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void ListBackups(google::cloud::bigtable::TableAdmin admin,
                 std::vector<std::string> const& argv) {
  //! [list backups]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string cluster_id, std::string filter,
     std::string order_by) {
    cbt::TableAdmin::ListBackupsParams list_backups_params;
    list_backups_params.set_cluster(std::move(cluster_id));
    list_backups_params.set_filter(std::move(filter));
    list_backups_params.set_order_by(std::move(order_by));
    StatusOr<std::vector<google::bigtable::admin::v2::Backup>> backups =
        admin.ListBackups(std::move(list_backups_params));
    if (!backups) throw std::runtime_error(backups.status().message());
    for (auto const& backup : *backups) {
      std::cout << backup.name() << "\n";
    }
  }
  //! [list backups]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2));
}

void GetBackup(google::cloud::bigtable::TableAdmin admin,
               std::vector<std::string> const& argv) {
  //! [get backup]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string cluster_id, std::string backup_id) {
    StatusOr<google::bigtable::admin::v2::Backup> backup =
        admin.GetBackup(std::move(cluster_id), std::move(backup_id));
    if (!backup) throw std::runtime_error(backup.status().message());
    std::cout << backup->name() << " details=\n"
              << backup->DebugString() << "\n";
  }
  //! [get backup]
  (std::move(admin), argv.at(0), argv.at(1));
}

void DeleteBackup(google::cloud::bigtable::TableAdmin admin,
                  std::vector<std::string> const& argv) {
  //! [delete backup]
  namespace cbt = google::cloud::bigtable;
  [](cbt::TableAdmin admin, std::string cluster_id, std::string backup_id) {
    google::cloud::Status status =
        admin.DeleteBackup(std::move(cluster_id), std::move(backup_id));
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Backup successfully deleted\n";
  }
  //! [delete backup]
  (std::move(admin), argv.at(0), argv.at(1));
}

void UpdateBackup(google::cloud::bigtable::TableAdmin admin,
                  std::vector<std::string> const& argv) {
  //! [update backup]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string cluster_id, std::string backup_id,
     std::string expire_time_string) {
    google::protobuf::Timestamp expire_time;
    if (!google::protobuf::util::TimeUtil::FromString(expire_time_string,
                                                      &expire_time)) {
      throw std::runtime_error("Unable to parse expire_time:" +
                               expire_time_string);
    }

    StatusOr<google::bigtable::admin::v2::Backup> backup =
        admin.UpdateBackup(cbt::TableAdmin::UpdateBackupParams(
            std::move(cluster_id), std::move(backup_id),
            std::move(expire_time)));
    if (!backup) throw std::runtime_error(backup.status().message());
    std::cout << backup->name() << " details=\n"
              << backup->DebugString() << "\n";
  }
  //! [update backup]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2));
}

void RestoreTable(google::cloud::bigtable::TableAdmin admin,
                  std::vector<std::string> const& argv) {
  //! [restore table]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id, std::string cluster_id,
     std::string backup_id) {
    StatusOr<google::bigtable::admin::v2::Table> table =
        admin.RestoreTable(cbt::TableAdmin::RestoreTableParams(
            std::move(table_id), std::move(cluster_id), std::move(backup_id)));
    if (!table) throw std::runtime_error(table.status().message());
    std::cout << "Table successfully restored: " << table->DebugString()
              << "\n";
  }
  //! [restore table]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::bigtable::examples;
  namespace cbt = google::cloud::bigtable;

  if (!argv.empty()) throw examples::Usage{"auto"};
  if (!examples::RunAdminIntegrationTests()) return;
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID",
      "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT",
      "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_CLUSTER_ID",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const instance_id = google::cloud::internal::GetEnv(
                               "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID")
                               .value();
  auto const service_account =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT")
          .value();
  auto const cluster_id = google::cloud::internal::GetEnv(
                              "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_CLUSTER_ID")
                              .value();

  cbt::TableAdmin admin(
      cbt::CreateDefaultAdminClient(project_id, cbt::ClientOptions{}),
      instance_id);

  // If a previous run of these samples crashes before cleaning up there may be
  // old tables left over. As there are quotas on the total number of tables we
  // remove stale tables after 48 hours.
  std::cout << "\nCleaning up old tables" << std::endl;
  std::string const prefix = "table-admin-snippets-";
  examples::CleanupOldTables(prefix, admin);
  std::string const backup_prefix = "table-admin-snippets-backup-";
  examples::CleanupOldBackups(cluster_id, admin);

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  // This table is actually created and used to test the positive case (e.g.
  // GetTable() and "table does exist")
  auto table_id_1 = examples::RandomTableId(prefix, generator);

  auto table_1 = admin.CreateTable(
      table_id_1, cbt::TableConfig(
                      {
                          {"fam", cbt::GcRule::MaxNumVersions(10)},
                          {"foo", cbt::GcRule::MaxNumVersions(3)},
                      },
                      {}));
  if (!table_1) throw std::runtime_error(table_1.status().message());

  std::cout << "\nRunning CreateBackup() example" << std::endl;
  auto backup_id_1 = examples::RandomTableId(backup_prefix, generator);
  CreateBackup(admin,
               {table_id_1, cluster_id, backup_id_1,
                google::protobuf::util::TimeUtil::ToString(
                    google::protobuf::util::TimeUtil::GetCurrentTime() +
                    google::protobuf::util::TimeUtil::HoursToDuration(12))});

  std::cout << "\nRunning ListBackups() example" << std::endl;
  ListBackups(admin, {"-", {}, {}});

  std::cout << "\nRunning GetBackup() example" << std::endl;
  GetBackup(admin, {cluster_id, backup_id_1});

  std::cout << "\nRunning UpdateBackup() example" << std::endl;
  UpdateBackup(admin,
               {cluster_id, backup_id_1,
                google::protobuf::util::TimeUtil::ToString(
                    google::protobuf::util::TimeUtil::GetCurrentTime() +
                    google::protobuf::util::TimeUtil::HoursToDuration(24))});

  (void)admin.DeleteTable(table_id_1);

  std::cout << "\nRunning RestoreTable() example" << std::endl;
  RestoreTable(admin, {table_id_1, cluster_id, backup_id_1});

  std::cout << "\nRunning DeleteBackup() example" << std::endl;
  DeleteBackup(admin, {cluster_id, backup_id_1});

  (void)admin.DeleteTable(table_id_1);
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  namespace examples = google::cloud::bigtable::examples;
  google::cloud::bigtable::examples::Example example({
      examples::MakeCommandEntry(
          "create-backup",
          {"<table-id>", "<cluster-id>", "<backup-id>", "<expire_time>"},
          CreateBackup),
      examples::MakeCommandEntry("list-backups",
                                 {"<cluster-id>", "<filter>", "<order_by>"},
                                 ListBackups),
      examples::MakeCommandEntry("get-backup", {"<cluster-id>", "<backup-id>"},
                                 GetBackup),
      examples::MakeCommandEntry("delete-backup",
                                 {"<cluster-id>", "<table-id>"}, DeleteBackup),
      examples::MakeCommandEntry("update-backup",
                                 {"<cluster-id>", "<backup-id>",
                                  "<expire-time(1980-06-20T00:00:00Z)>"},
                                 UpdateBackup),
      examples::MakeCommandEntry("restore-table",
                                 {"<table-id>", "<cluster-id>", "<backup-id>"},
                                 RestoreTable),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
