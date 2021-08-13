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
#include "google/cloud/bigtable/testing/cleanup_stale_resources.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/internal/getenv.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include <google/protobuf/util/time_util.h>
#include <sstream>

namespace {

using ::google::cloud::bigtable::examples::Usage;

void CreateBackup(google::cloud::bigtable::TableAdmin const& admin,
                  std::vector<std::string> const& argv) {
  //! [create backup]
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id,
     std::string const& cluster_id, std::string const& backup_id,
     std::string const& expire_time_string) {
    absl::Time t;
    std::string err;
    if (!absl::ParseTime(absl::RFC3339_full, expire_time_string, &t, &err)) {
      throw std::runtime_error("Unable to parse expire_time:" + err);
    }
    auto expire_time = absl::ToChronoTime(t);
    StatusOr<google::bigtable::admin::v2::Backup> backup =
        admin.CreateBackup(cbt::TableAdmin::CreateBackupParams(
            cluster_id, backup_id, table_id, expire_time));
    if (!backup) throw std::runtime_error(backup.status().message());
    std::cout << "Backup successfully created: " << backup->DebugString()
              << "\n";
  }
  //! [create backup]
  (admin, argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void ListBackups(google::cloud::bigtable::TableAdmin const& admin,
                 std::vector<std::string> const& argv) {
  //! [list backups]
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& cluster_id,
     std::string const& filter, std::string const& order_by) {
    cbt::TableAdmin::ListBackupsParams list_backups_params;
    list_backups_params.set_cluster(cluster_id);
    list_backups_params.set_filter(filter);
    list_backups_params.set_order_by(order_by);
    StatusOr<std::vector<google::bigtable::admin::v2::Backup>> backups =
        admin.ListBackups(std::move(list_backups_params));
    if (!backups) throw std::runtime_error(backups.status().message());
    for (auto const& backup : *backups) {
      std::cout << backup.name() << "\n";
    }
  }
  //! [list backups]
  (admin, argv.at(0), argv.at(1), argv.at(2));
}

void GetBackup(google::cloud::bigtable::TableAdmin const& admin,
               std::vector<std::string> const& argv) {
  //! [get backup]
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& cluster_id,
     std::string const& backup_id) {
    StatusOr<google::bigtable::admin::v2::Backup> backup =
        admin.GetBackup(cluster_id, backup_id);
    if (!backup) throw std::runtime_error(backup.status().message());
    std::cout << backup->name() << " details=\n"
              << backup->DebugString() << "\n";
  }
  //! [get backup]
  (admin, argv.at(0), argv.at(1));
}

void DeleteBackup(google::cloud::bigtable::TableAdmin const& admin,
                  std::vector<std::string> const& argv) {
  //! [delete backup]
  namespace cbt = ::google::cloud::bigtable;
  [](cbt::TableAdmin admin, std::string const& cluster_id,
     std::string const& backup_id) {
    google::cloud::Status status = admin.DeleteBackup(cluster_id, backup_id);
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Backup successfully deleted\n";
  }
  //! [delete backup]
  (admin, argv.at(0), argv.at(1));
}

void UpdateBackup(google::cloud::bigtable::TableAdmin const& admin,
                  std::vector<std::string> const& argv) {
  //! [update backup]
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& cluster_id,
     std::string const& backup_id, std::string const& expire_time_string) {
    absl::Time t;
    std::string err;
    if (!absl::ParseTime(absl::RFC3339_full, expire_time_string, &t, &err)) {
      throw std::runtime_error("Unable to parse expire_time:" + err);
    }
    auto expire_time = absl::ToChronoTime(t);

    StatusOr<google::bigtable::admin::v2::Backup> backup =
        admin.UpdateBackup(cbt::TableAdmin::UpdateBackupParams(
            cluster_id, backup_id, expire_time));
    if (!backup) throw std::runtime_error(backup.status().message());
    std::cout << backup->name() << " details=\n"
              << backup->DebugString() << "\n";
  }
  //! [update backup]
  (admin, argv.at(0), argv.at(1), argv.at(2));
}

void RestoreTable(google::cloud::bigtable::TableAdmin const& admin,
                  std::vector<std::string> const& argv) {
  //! [restore table]
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id,
     std::string const& cluster_id, std::string const& backup_id) {
    StatusOr<google::bigtable::admin::v2::Table> table = admin.RestoreTable(
        cbt::TableAdmin::RestoreTableParams(table_id, cluster_id, backup_id));
    if (!table) throw std::runtime_error(table.status().message());
    std::cout << "Table successfully restored: " << table->DebugString()
              << "\n";
  }
  //! [restore table]
  (admin, argv.at(0), argv.at(1), argv.at(2));
}

void RestoreTableFromInstance(google::cloud::bigtable::TableAdmin const& admin,
                              std::vector<std::string> const& argv) {
  //! [restore2]
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id,
     std::string const& other_instance_id, std::string const& cluster_id,
     std::string const& backup_id) {
    StatusOr<google::bigtable::admin::v2::Table> table =
        admin.RestoreTable(cbt::TableAdmin::RestoreTableFromInstanceParams{
            table_id, cbt::BackupName(admin.project(), other_instance_id,
                                      cluster_id, backup_id)});
    if (!table) throw std::runtime_error(table.status().message());
    std::cout << "Table successfully restored: " << table->DebugString()
              << "\n";
  }
  //! [restore2]
  (admin, argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void GetIamPolicy(google::cloud::bigtable::TableAdmin const& admin,
                  std::vector<std::string> const& argv) {
  //! [get backup iam policy]
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& cluster_id,
     std::string const& backup_id) {
    StatusOr<google::iam::v1::Policy> policy =
        admin.GetIamPolicy(cluster_id, backup_id);
    if (!policy) throw std::runtime_error(policy.status().message());
    std::cout << "The IAM Policy is:\n" << policy->DebugString() << "\n";
  }
  //! [get backup iam policy]
  (admin, argv.at(0), argv.at(1));
}

void SetIamPolicy(google::cloud::bigtable::TableAdmin const& admin,
                  std::vector<std::string> const& argv) {
  //! [set backup iam policy]
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& cluster_id,
     std::string const& backup_id, std::string const& role,
     std::string const& member) {
    StatusOr<google::iam::v1::Policy> current =
        admin.GetIamPolicy(cluster_id, backup_id);
    if (!current) throw std::runtime_error(current.status().message());
    // This example adds the member to all existing bindings for that role. If
    // there are no such bindings, it adds a new one. This might not be what the
    // user wants, e.g. in case of conditional bindings.
    size_t num_added = 0;
    for (auto& binding : *current->mutable_bindings()) {
      if (binding.role() == role) {
        binding.add_members(member);
        ++num_added;
      }
    }
    if (num_added == 0) {
      *current->add_bindings() = cbt::IamBinding(role, {member});
    }
    StatusOr<google::iam::v1::Policy> policy =
        admin.SetIamPolicy(cluster_id, backup_id, *current);
    if (!policy) throw std::runtime_error(policy.status().message());
    std::cout << "The IAM Policy is:\n" << policy->DebugString() << "\n";
  }
  //! [set backup iam policy]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::bigtable::examples;
  namespace cbt = ::google::cloud::bigtable;

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
  google::cloud::bigtable::testing::CleanupStaleTables(admin);
  std::string const backup_prefix = "table-admin-snippets-backup-";
  google::cloud::bigtable::testing::CleanupStaleBackups(admin);

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  // This table is actually created and used to test the positive case (e.g.
  // GetTable() and "table does exist")
  auto table_id = google::cloud::bigtable::testing::RandomTableId(generator);

  auto table = admin.CreateTable(
      table_id, cbt::TableConfig(
                    {
                        {"fam", cbt::GcRule::MaxNumVersions(10)},
                        {"foo", cbt::GcRule::MaxNumVersions(3)},
                    },
                    {}));
  if (!table) throw std::runtime_error(table.status().message());

  std::cout << "\nRunning CreateBackup() example" << std::endl;
  auto backup_id = google::cloud::bigtable::testing::RandomTableId(generator);
  CreateBackup(admin, {table_id, cluster_id, backup_id,
                       absl::FormatTime(absl::Now() + absl::Hours(12))});

  std::cout << "\nRunning ListBackups() example" << std::endl;
  ListBackups(admin, {"-", {}, {}});

  std::cout << "\nRunning GetBackup() example" << std::endl;
  GetBackup(admin, {cluster_id, backup_id});

  std::cout << "\nRunning UpdateBackup() example" << std::endl;
  UpdateBackup(admin, {cluster_id, backup_id,
                       absl::FormatTime(absl::Now() + absl::Hours(24))});

  std::cout << "\nRunning SetIamPolicy() example" << std::endl;
  SetIamPolicy(admin, {cluster_id, backup_id, "roles/bigtable.user",
                       "serviceAccount:" + service_account});

  std::cout << "\nRunning GetIamPolicy() example" << std::endl;
  GetIamPolicy(admin, {cluster_id, backup_id});

  (void)admin.DeleteTable(table_id);

  std::cout << "\nRunning RestoreTable() example" << std::endl;
  RestoreTable(admin, {table_id, cluster_id, backup_id});

  (void)admin.DeleteTable(table_id);

  std::cout << "\nRunning RestoreTableFromInstance() example" << std::endl;
  RestoreTableFromInstance(admin,
                           {table_id, instance_id, cluster_id, backup_id});

  std::cout << "\nRunning DeleteBackup() example" << std::endl;
  DeleteBackup(admin, {cluster_id, backup_id});

  (void)admin.DeleteTable(table_id);
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::bigtable::examples;
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
      examples::MakeCommandEntry(
          "restore-table-from-instance",
          {"<table-id>", "<other-instance-id>", "<cluster-id>", "<backup-id>"},
          RestoreTableFromInstance),
      examples::MakeCommandEntry("get-iam-policy",
                                 {"<cluster-id>", "<backup_id>"}, GetIamPolicy),
      examples::MakeCommandEntry(
          "set-iam-policy",
          {"<cluster-id>", "<backup_id>", "<role>", "<member>"}, SetIamPolicy),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
