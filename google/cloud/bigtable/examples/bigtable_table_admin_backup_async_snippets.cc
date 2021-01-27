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
#include "google/cloud/internal/random.h"
#include "google/cloud/internal/time_utils.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include <algorithm>

namespace {

void AsyncCreateBackup(google::cloud::bigtable::TableAdmin const& admin,
                       google::cloud::bigtable::CompletionQueue cq,
                       std::vector<std::string> const& argv) {
  //! [async create backup]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq,
     std::string const& table_id, std::string const& cluster_id,
     std::string const& backup_id, std::string const& expire_time_string) {
    absl::Time t;
    std::string err;
    if (!absl::ParseTime(absl::RFC3339_full, expire_time_string, &t, &err)) {
      throw std::runtime_error("Unable to parse expire_time:" + err);
    }
    auto expire_time = absl::ToChronoTime(t);

    future<StatusOr<google::bigtable::admin::v2::Backup>> backup_future =
        admin.AsyncCreateBackup(
            cq, cbt::TableAdmin::CreateBackupParams(cluster_id, backup_id,
                                                    table_id, expire_time));

    auto final = backup_future.then(
        [](future<StatusOr<google::bigtable::admin::v2::Backup>> f) {
          auto backup = f.get();
          if (!backup) {
            throw std::runtime_error(backup.status().message());
          }
          std::cout << "Backup successfully created: " << backup->DebugString()
                    << "\n";
          return google::cloud::Status();
        });
    final.get();
  }
  //! [async create backup]
  (admin, std::move(cq), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void AsyncListBackups(google::cloud::bigtable::TableAdmin const& admin,
                      google::cloud::bigtable::CompletionQueue cq,
                      std::vector<std::string> const& argv) {
  //! [async list backups]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq,
     std::string const& cluster_id, std::string const& filter,
     std::string const& order_by) {
    cbt::TableAdmin::ListBackupsParams list_backups_params;
    list_backups_params.set_cluster(cluster_id);
    list_backups_params.set_filter(filter);
    list_backups_params.set_order_by(order_by);
    future<StatusOr<std::vector<google::bigtable::admin::v2::Backup>>>
        backups_future =
            admin.AsyncListBackups(cq, std::move(list_backups_params));

    auto final = backups_future.then(
        [](future<StatusOr<std::vector<google::bigtable::admin::v2::Backup>>>
               f) {
          auto backups = f.get();
          if (!backups) {
            throw std::runtime_error(backups.status().message());
          }
          for (auto const& backup : *backups) {
            std::cout << backup.name() << "\n";
          }
          return google::cloud::Status();
        });
    final.get();
  }
  //! [async list backups]
  (admin, std::move(cq), argv.at(0), argv.at(1), argv.at(2));
}

void AsyncGetBackup(google::cloud::bigtable::TableAdmin const& admin,
                    google::cloud::bigtable::CompletionQueue cq,
                    std::vector<std::string> const& argv) {
  //! [async get backup]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq,
     std::string const& cluster_id, std::string const& backup_id) {
    future<StatusOr<google::bigtable::admin::v2::Backup>> backup_future =
        admin.AsyncGetBackup(cq, cluster_id, backup_id);

    auto final = backup_future.then(
        [](future<StatusOr<google::bigtable::admin::v2::Backup>> f) {
          auto backup = f.get();
          if (!backup) {
            throw std::runtime_error(backup.status().message());
          }
          std::cout << backup->name() << " details=\n"
                    << backup->DebugString() << "\n";
        });
    final.get();
  }
  //! [async get backup]
  (admin, std::move(cq), argv.at(0), argv.at(1));
}

void AsyncDeleteBackup(google::cloud::bigtable::TableAdmin const& admin,
                       google::cloud::bigtable::CompletionQueue cq,
                       std::vector<std::string> const& argv) {
  //! [async delete backup]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq,
     std::string const& cluster_id, std::string const& backup_id) {
    future<google::cloud::Status> status_future =
        admin.AsyncDeleteBackup(cq, cluster_id, backup_id);

    auto final = status_future.then([](future<google::cloud::Status> f) {
      auto status = f.get();

      if (!status.ok()) {
        throw std::runtime_error(status.message());
      }
      std::cout << "Backup successfully deleted\n";
    });
    final.get();
  }
  //! [async delete backup]
  (admin, std::move(cq), argv.at(0), argv.at(1));
}

void AsyncUpdateBackup(google::cloud::bigtable::TableAdmin const& admin,
                       google::cloud::bigtable::CompletionQueue cq,
                       std::vector<std::string> const& argv) {
  //! [async update backup]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq,
     std::string const& cluster_id, std::string const& backup_id,
     std::string const& expire_time_string) {
    absl::Time t;
    std::string err;
    if (!absl::ParseTime(absl::RFC3339_full, expire_time_string, &t, &err)) {
      throw std::runtime_error("Unable to parse expire_time:" + err);
    }
    auto expire_time = absl::ToChronoTime(t);

    future<StatusOr<google::bigtable::admin::v2::Backup>> backup_future =
        admin.AsyncUpdateBackup(cq, cbt::TableAdmin::UpdateBackupParams(
                                        cluster_id, backup_id, expire_time));

    auto final = backup_future.then(
        [](future<StatusOr<google::bigtable::admin::v2::Backup>> f) {
          auto backup = f.get();
          if (!backup) {
            throw std::runtime_error(backup.status().message());
          }
          std::cout << backup->name() << " details=\n"
                    << backup->DebugString() << "\n";
        });
    final.get();
  }
  //! [async update backup]
  (admin, std::move(cq), argv.at(0), argv.at(1), argv.at(2));
}

void AsyncRestoreTable(google::cloud::bigtable::TableAdmin const& admin,
                       google::cloud::bigtable::CompletionQueue cq,
                       std::vector<std::string> const& argv) {
  //! [async restore table]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq,
     std::string const& table_id, std::string const& cluster_id,
     std::string const& backup_id) {
    future<StatusOr<google::bigtable::admin::v2::Table>> table_future =
        admin.AsyncRestoreTable(cq, cbt::TableAdmin::RestoreTableParams(
                                        table_id, cluster_id, backup_id));

    auto final = table_future.then(
        [](future<StatusOr<google::bigtable::admin::v2::Table>> f) {
          auto table = f.get();

          if (!table) {
            throw std::runtime_error(table.status().message());
          }
          std::cout << "Table successfully restored: " << table->DebugString()
                    << "\n";
        });
    final.get();
  }
  //! [async restore table]
  (admin, std::move(cq), argv.at(0), argv.at(1), argv.at(2));
}

void AsyncRestoreTableFromInstance(
    google::cloud::bigtable::TableAdmin const& admin,
    google::cloud::bigtable::CompletionQueue cq,
    std::vector<std::string> const& argv) {
  //! [async restore2]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq,
     std::string const& table_id, std::string const& other_instance_id,
     std::string const& cluster_id, std::string const& backup_id) {
    future<StatusOr<google::bigtable::admin::v2::Table>> table_future =
        admin.AsyncRestoreTable(
            cq,
            cbt::TableAdmin::RestoreTableFromInstanceParams{
                table_id, cbt::BackupName(admin.project(), other_instance_id,
                                          cluster_id, backup_id)});

    auto final = table_future.then(
        [](future<StatusOr<google::bigtable::admin::v2::Table>> f) {
          auto table = f.get();

          if (!table) {
            throw std::runtime_error(table.status().message());
          }
          std::cout << "Table successfully restored: " << table->DebugString()
                    << "\n";
        });
    final.get();
  }
  //! [async restore2]
  (admin, std::move(cq), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

}  // anonymous namespace

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

  google::cloud::CompletionQueue cq;
  std::thread th([&cq] { cq.Run(); });
  examples::AutoShutdownCQ shutdown(cq, std::move(th));

  // If a previous run of these samples crashes before cleaning up there may be
  // old tables left over. As there are quotas on the total number of tables we
  // remove stale tables after 48 hours.
  std::cout << "\nCleaning up old tables" << std::endl;
  std::string const prefix = "table-admin-snippets-";
  examples::CleanupOldTables(prefix, admin);
  std::string const backup_prefix = "table-admin-snippets-backup-";
  examples::CleanupOldBackups(cluster_id, admin);

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto table_id = examples::RandomTableId(prefix, generator);

  auto table = admin.CreateTable(
      table_id, cbt::TableConfig(
                    {
                        {"fam", cbt::GcRule::MaxNumVersions(10)},
                        {"foo", cbt::GcRule::MaxNumVersions(3)},
                    },
                    {}));
  if (!table) throw std::runtime_error(table.status().message());

  std::cout << "\nRunning AsyncCreateBackup() example" << std::endl;
  auto backup_id = examples::RandomTableId(backup_prefix, generator);
  AsyncCreateBackup(admin, cq,
                    {table_id, cluster_id, backup_id,
                     absl::FormatTime(absl::Now() + absl::Hours(12))});

  std::cout << "\nRunning AsyncListBackups() example" << std::endl;
  AsyncListBackups(admin, cq, {"-", {}, {}});

  std::cout << "\nRunning AsyncGetBackup() example" << std::endl;
  AsyncGetBackup(admin, cq, {cluster_id, backup_id});

  std::cout << "\nRunning AsyncUpdateBackup() example" << std::endl;
  AsyncUpdateBackup(
      admin, cq,
      {cluster_id, backup_id, absl::FormatTime(absl::Now() + absl::Hours(24))});

  (void)admin.DeleteTable(table_id);

  std::cout << "\nRunning AsyncRestoreTable() example" << std::endl;
  AsyncRestoreTable(admin, cq, {table_id, cluster_id, backup_id});

  (void)admin.DeleteTable(table_id);

  std::cout << "\nRunning AsyncRestoreTableFromInstance() example" << std::endl;
  AsyncRestoreTableFromInstance(admin, cq,
                                {table_id, instance_id, cluster_id, backup_id});

  std::cout << "\nRunning AsyncDeleteBackup() example" << std::endl;
  AsyncDeleteBackup(admin, cq, {cluster_id, backup_id});

  (void)admin.DeleteTable(table_id);
}

int main(int argc, char* argv[]) {
  namespace examples = google::cloud::bigtable::examples;
  examples::Example example({
      examples::MakeCommandEntry(
          "async-create-backup",
          {"<table-id>", "<cluster-id>", "<backup-id>", "<expire_time>"},
          AsyncCreateBackup),
      examples::MakeCommandEntry("async-list-backups",
                                 {"<cluster-id>", "<filter>", "<order_by>"},
                                 AsyncListBackups),
      examples::MakeCommandEntry(
          "async-get-backup", {"<cluster-id>", "<backup-id>"}, AsyncGetBackup),
      examples::MakeCommandEntry("async-delete-backup",
                                 {"<cluster-id>", "<table-id>"},
                                 AsyncDeleteBackup),
      examples::MakeCommandEntry("async-update-backup",
                                 {"<cluster-id>", "<backup-id>",
                                  "<expire-time(1980-06-20T00:00:00Z)>"},
                                 AsyncUpdateBackup),
      examples::MakeCommandEntry("async-restore-table",
                                 {"<table-id>", "<cluster-id>", "<backup-id>"},
                                 AsyncRestoreTable),
      examples::MakeCommandEntry(
          "async-restore-table-from-instance",
          {"<table-id>", "<other-instance>", "<cluster-id>", "<backup-id>"},
          AsyncRestoreTableFromInstance),

      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
