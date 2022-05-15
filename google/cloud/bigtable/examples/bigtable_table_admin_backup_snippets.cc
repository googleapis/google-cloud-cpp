// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/admin/bigtable_table_admin_client.h"
#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/bigtable/iam_binding.h"
#include "google/cloud/bigtable/resource_names.h"
#include "google/cloud/bigtable/testing/cleanup_stale_resources.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/internal/getenv.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include <sstream>

namespace {

using ::google::cloud::bigtable::examples::Usage;

void CreateBackup(google::cloud::bigtable_admin::BigtableTableAdminClient admin,
                  std::vector<std::string> const& argv) {
  //! [create backup]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id,
     std::string const& cluster_id, std::string const& backup_id,
     std::string const& expire_time_string) {
    absl::Time t;
    std::string err;
    if (!absl::ParseTime(absl::RFC3339_full, expire_time_string, &t, &err)) {
      throw std::runtime_error("Unable to parse expire_time:" + err);
    }
    std::string cluster_name =
        cbt::ClusterName(project_id, instance_id, cluster_id);
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);

    google::bigtable::admin::v2::Backup b;
    b.set_source_table(table_name);
    b.mutable_expire_time()->set_seconds(absl::ToUnixSeconds(t));

    future<StatusOr<google::bigtable::admin::v2::Backup>> backup_future =
        admin.CreateBackup(cluster_name, backup_id, std::move(b));
    auto backup = backup_future.get();
    if (!backup) throw std::runtime_error(backup.status().message());
    std::cout << "Backup successfully created: " << backup->DebugString()
              << "\n";
  }
  //! [create backup]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3), argv.at(4),
   argv.at(5));
}

void ListBackups(google::cloud::bigtable_admin::BigtableTableAdminClient admin,
                 std::vector<std::string> const& argv) {
  //! [list backups]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StreamRange;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& cluster_id,
     std::string const& filter, std::string const& order_by) {
    std::string cluster_name =
        cbt::ClusterName(project_id, instance_id, cluster_id);

    google::bigtable::admin::v2::ListBackupsRequest r;
    r.set_parent(cluster_name);
    r.set_filter(filter);
    r.set_order_by(order_by);

    StreamRange<google::bigtable::admin::v2::Backup> backups =
        admin.ListBackups(std::move(r));
    for (auto const& backup : backups) {
      if (!backup) throw std::runtime_error(backup.status().message());
      std::cout << backup->name() << "\n";
    }
  }
  //! [list backups]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3),
   argv.at(4));
}

void GetBackup(google::cloud::bigtable_admin::BigtableTableAdminClient admin,
               std::vector<std::string> const& argv) {
  //! [get backup]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& cluster_id,
     std::string const& backup_id) {
    std::string backup_name =
        cbt::BackupName(project_id, instance_id, cluster_id, backup_id);
    StatusOr<google::bigtable::admin::v2::Backup> backup =
        admin.GetBackup(backup_name);
    if (!backup) throw std::runtime_error(backup.status().message());
    std::cout << backup->name() << " details=\n"
              << backup->DebugString() << "\n";
  }
  //! [get backup]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void DeleteBackup(google::cloud::bigtable_admin::BigtableTableAdminClient admin,
                  std::vector<std::string> const& argv) {
  //! [delete backup]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::Status;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& cluster_id,
     std::string const& backup_id) {
    std::string backup_name =
        cbt::BackupName(project_id, instance_id, cluster_id, backup_id);
    Status status = admin.DeleteBackup(backup_name);
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Backup successfully deleted\n";
  }
  //! [delete backup]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void UpdateBackup(google::cloud::bigtable_admin::BigtableTableAdminClient admin,
                  std::vector<std::string> const& argv) {
  //! [update backup]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& cluster_id,
     std::string const& backup_id, std::string const& expire_time_string) {
    absl::Time t;
    std::string err;
    if (!absl::ParseTime(absl::RFC3339_full, expire_time_string, &t, &err)) {
      throw std::runtime_error("Unable to parse expire_time:" + err);
    }
    std::string backup_name =
        cbt::BackupName(project_id, instance_id, cluster_id, backup_id);

    google::bigtable::admin::v2::Backup b;
    b.set_name(backup_name);
    b.mutable_expire_time()->set_seconds(absl::ToUnixSeconds(t));

    google::protobuf::FieldMask mask;
    mask.add_paths("expire_time");

    StatusOr<google::bigtable::admin::v2::Backup> backup =
        admin.UpdateBackup(std::move(b), std::move(mask));
    if (!backup) throw std::runtime_error(backup.status().message());
    std::cout << backup->name() << " details=\n"
              << backup->DebugString() << "\n";
  }
  //! [update backup]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3),
   argv.at(4));
}

void RestoreTable(google::cloud::bigtable_admin::BigtableTableAdminClient admin,
                  std::vector<std::string> const& argv) {
  //! [restore table]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id,
     std::string const& cluster_id, std::string const& backup_id) {
    std::string instance_name = cbt::InstanceName(project_id, instance_id);
    std::string backup_name =
        cbt::BackupName(project_id, instance_id, cluster_id, backup_id);

    google::bigtable::admin::v2::RestoreTableRequest r;
    r.set_parent(instance_name);
    r.set_table_id(table_id);
    r.set_backup(backup_name);

    future<StatusOr<google::bigtable::admin::v2::Table>> table_future =
        admin.RestoreTable(std::move(r));
    auto table = table_future.get();
    if (!table) throw std::runtime_error(table.status().message());
    std::cout << "Table successfully restored: " << table->DebugString()
              << "\n";
  }
  //! [restore table]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3),
   argv.at(4));
}

void RestoreTableFromInstance(
    google::cloud::bigtable_admin::BigtableTableAdminClient admin,
    std::vector<std::string> const& argv) {
  //! [restore2]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id,
     std::string const& other_instance_id, std::string const& cluster_id,
     std::string const& backup_id) {
    std::string instance_name = cbt::InstanceName(project_id, instance_id);
    std::string backup_name =
        cbt::BackupName(project_id, other_instance_id, cluster_id, backup_id);

    google::bigtable::admin::v2::RestoreTableRequest r;
    r.set_parent(instance_name);
    r.set_table_id(table_id);
    r.set_backup(backup_name);

    future<StatusOr<google::bigtable::admin::v2::Table>> table_future =
        admin.RestoreTable(std::move(r));
    auto table = table_future.get();
    if (!table) throw std::runtime_error(table.status().message());
    std::cout << "Table successfully restored: " << table->DebugString()
              << "\n";
  }
  //! [restore2]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3), argv.at(4),
   argv.at(5));
}

void GetIamPolicy(google::cloud::bigtable_admin::BigtableTableAdminClient admin,
                  std::vector<std::string> const& argv) {
  //! [get backup iam policy]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& cluster_id,
     std::string const& backup_id) {
    std::string backup_name =
        cbt::BackupName(project_id, instance_id, cluster_id, backup_id);
    StatusOr<google::iam::v1::Policy> policy = admin.GetIamPolicy(backup_name);
    if (!policy) throw std::runtime_error(policy.status().message());
    std::cout << "The IAM Policy is:\n" << policy->DebugString() << "\n";
  }
  //! [get backup iam policy]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void SetIamPolicy(google::cloud::bigtable_admin::BigtableTableAdminClient admin,
                  std::vector<std::string> const& argv) {
  //! [set backup iam policy]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& cluster_id,
     std::string const& backup_id, std::string const& role,
     std::string const& member) {
    std::string backup_name =
        cbt::BackupName(project_id, instance_id, cluster_id, backup_id);
    StatusOr<google::iam::v1::Policy> current = admin.GetIamPolicy(backup_name);
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
        admin.SetIamPolicy(backup_name, *current);
    if (!policy) throw std::runtime_error(policy.status().message());
    std::cout << "The IAM Policy is:\n" << policy->DebugString() << "\n";
  }
  //! [set backup iam policy]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3), argv.at(4),
   argv.at(5));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::bigtable::examples;
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;

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

  auto conn = cbta::MakeBigtableTableAdminConnection();
  // If a previous run of these samples crashes before cleaning up there may be
  // old tables left over. As there are quotas on the total number of tables we
  // remove stale tables after 48 hours.
  std::cout << "\nCleaning up old tables" << std::endl;
  cbt::testing::CleanupStaleTables(conn, project_id, instance_id);
  cbt::testing::CleanupStaleBackups(conn, project_id, instance_id);
  auto admin = cbta::BigtableTableAdminClient(std::move(conn));

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto table_id = cbt::testing::RandomTableId(generator);

  // Create a table to run the tests on.
  google::bigtable::admin::v2::Table t;
  auto& families = *t.mutable_column_families();
  families["fam"].mutable_gc_rule()->set_max_num_versions(10);
  families["foo"].mutable_gc_rule()->set_max_num_versions(3);

  auto table = admin.CreateTable(cbt::InstanceName(project_id, instance_id),
                                 table_id, std::move(t));
  if (!table) throw std::runtime_error(table.status().message());

  std::cout << "\nRunning CreateBackup() example" << std::endl;
  auto backup_id = google::cloud::bigtable::testing::RandomTableId(generator);
  CreateBackup(admin, {project_id, instance_id, table_id, cluster_id, backup_id,
                       absl::FormatTime(absl::Now() + absl::Hours(12))});

  std::cout << "\nRunning ListBackups() example" << std::endl;
  ListBackups(admin, {project_id, instance_id, "-", {}, {}});

  std::cout << "\nRunning GetBackup() example" << std::endl;
  GetBackup(admin, {project_id, instance_id, cluster_id, backup_id});

  std::cout << "\nRunning UpdateBackup() example" << std::endl;
  UpdateBackup(admin, {project_id, instance_id, cluster_id, backup_id,
                       absl::FormatTime(absl::Now() + absl::Hours(24))});

  std::cout << "\nRunning SetIamPolicy() example" << std::endl;
  SetIamPolicy(admin,
               {project_id, instance_id, cluster_id, backup_id,
                "roles/bigtable.user", "serviceAccount:" + service_account});

  std::cout << "\nRunning GetIamPolicy() example" << std::endl;
  GetIamPolicy(admin, {project_id, instance_id, cluster_id, backup_id});

  (void)admin.DeleteTable(table->name());

  std::cout << "\nRunning RestoreTable() example" << std::endl;
  RestoreTable(admin,
               {project_id, instance_id, table_id, cluster_id, backup_id});

  (void)admin.DeleteTable(table->name());

  std::cout << "\nRunning RestoreTableFromInstance() example" << std::endl;
  RestoreTableFromInstance(admin, {project_id, instance_id, table_id,
                                   instance_id, cluster_id, backup_id});

  std::cout << "\nRunning DeleteBackup() example" << std::endl;
  DeleteBackup(admin, {project_id, instance_id, cluster_id, backup_id});

  (void)admin.DeleteTable(table->name());
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::bigtable::examples;
  google::cloud::bigtable::examples::Example example({
      examples::MakeCommandEntry("create-backup",
                                 {"<table-id>", "<cluster-id>", "<backup-id>",
                                  "<expire-time(1980-06-20T00:00:00Z)>"},
                                 CreateBackup),
      examples::MakeCommandEntry("list-backups",
                                 {"<cluster-id>", "<filter>", "<order-by>"},
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
                                 {"<cluster-id>", "<backup-id>"}, GetIamPolicy),
      examples::MakeCommandEntry(
          "set-iam-policy",
          {"<cluster-id>", "<backup-id>", "<role>", "<member>"}, SetIamPolicy),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
