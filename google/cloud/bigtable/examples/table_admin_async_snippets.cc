// Copyright 2018 Google LLC
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
#include "google/cloud/testing_util/crash_handler.h"

namespace {

using ::google::cloud::bigtable::examples::Usage;

void AsyncCreateTable(google::cloud::bigtable::TableAdmin const& admin,
                      google::cloud::bigtable::CompletionQueue cq,
                      std::vector<std::string> const& argv) {
  //! [async create table]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq,
     std::string const& table_id) {
    future<StatusOr<google::bigtable::admin::v2::Table>> table_future =
        admin.AsyncCreateTable(
            cq, table_id,
            cbt::TableConfig(
                {{"fam", cbt::GcRule::MaxNumVersions(10)},
                 {"foo", cbt::GcRule::MaxAge(std::chrono::hours(72))}},
                {}));

    auto final = table_future.then(
        [](future<StatusOr<google::bigtable::admin::v2::Table>> f) {
          auto table = f.get();
          if (!table) throw std::runtime_error(table.status().message());
          std::cout << "Table created as " << table->name() << "\n";
          return google::cloud::Status();
        });
    final.get();  // block to simplify the example.
  }
  //! [async create table]
  (std::move(admin), std::move(cq), argv.at(0));
}

void AsyncListTables(google::cloud::bigtable::TableAdmin const& admin,
                     google::cloud::bigtable::CompletionQueue cq,
                     std::vector<std::string> const&) {
  //! [async list tables]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq) {
    future<StatusOr<std::vector<google::bigtable::admin::v2::Table>>>
        tables_future = admin.AsyncListTables(cq, cbt::TableAdmin::NAME_ONLY);

    auto final = tables_future.then(
        [](future<StatusOr<std::vector<google::bigtable::admin::v2::Table>>>
               f) {
          auto tables = f.get();
          if (!tables) throw std::runtime_error(tables.status().message());
          for (auto const& table : *tables) {
            std::cout << table.name() << "\n";
          }
          return google::cloud::Status();
        });
    final.get();  // block to simplify the example.
  }
  //! [async list tables]
  (std::move(admin), std::move(cq));
}

void AsyncGetTable(google::cloud::bigtable::TableAdmin const& admin,
                   google::cloud::bigtable::CompletionQueue cq,
                   std::vector<std::string> const& argv) {
  //! [async get table]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq,
     std::string const& table_id) {
    future<StatusOr<google::bigtable::admin::v2::Table>> table_future =
        admin.AsyncGetTable(cq, table_id,
                            google::bigtable::admin::v2::Table::FULL);

    auto final = table_future.then(
        [](future<StatusOr<google::bigtable::admin::v2::Table>> f) {
          auto table = f.get();
          if (!table) throw std::runtime_error(table.status().message());
          std::cout << table->name() << "\n";
          for (auto const& family : table->column_families()) {
            std::string const& family_name = family.first;
            std::cout << "\t" << family_name << "\t\t"
                      << family.second.DebugString() << "\n";
          }
          return google::cloud::Status();
        });

    final.get();  // block to simplify the example.
  }
  //! [async get table]
  (std::move(admin), std::move(cq), argv.at(0));
}

void AsyncDeleteTable(google::cloud::bigtable::TableAdmin const& admin,
                      google::cloud::bigtable::CompletionQueue cq,
                      std::vector<std::string> const& argv) {
  //! [async delete table]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq,
     std::string const& table_id) {
    future<google::cloud::Status> status_future =
        admin.AsyncDeleteTable(cq, table_id);

    auto final =
        status_future.then([table_id](future<google::cloud::Status> f) {
          auto status = f.get();
          if (!status.ok()) throw std::runtime_error(status.message());
          std::cout << "Successfully deleted table: " << table_id << "\n";
        });

    final.get();  // block to simplify example.
  }
  //! [async delete table]
  (std::move(admin), std::move(cq), argv.at(0));
}

void AsyncModifyTable(google::cloud::bigtable::TableAdmin const& admin,
                      google::cloud::bigtable::CompletionQueue cq,
                      std::vector<std::string> const& argv) {
  //! [async modify table]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq,
     std::string const& table_id) {
    future<StatusOr<google::bigtable::admin::v2::Table>> table_future =
        admin.AsyncModifyColumnFamilies(
            cq, table_id,
            {cbt::ColumnFamilyModification::Drop("foo"),
             cbt::ColumnFamilyModification::Update(
                 "fam", cbt::GcRule::Union(
                            cbt::GcRule::MaxNumVersions(5),
                            cbt::GcRule::MaxAge(std::chrono::hours(24 * 7)))),
             cbt::ColumnFamilyModification::Create(
                 "bar", cbt::GcRule::Intersection(
                            cbt::GcRule::MaxNumVersions(3),
                            cbt::GcRule::MaxAge(std::chrono::hours(72))))});

    auto final = table_future.then(
        [](future<StatusOr<google::bigtable::admin::v2::Table>> f) {
          auto table = f.get();
          if (!table) throw std::runtime_error(table.status().message());
          std::cout << table->name() << ":\n";
          std::cout << table->DebugString() << "\n";
        });

    final.get();  // block to simplify example.
  }
  //! [async modify table]
  (std::move(admin), std::move(cq), argv.at(0));
}

void AsyncDropRowsByPrefix(google::cloud::bigtable::TableAdmin const& admin,
                           google::cloud::bigtable::CompletionQueue cq,
                           std::vector<std::string> const& argv) {
  //! [async drop rows by prefix]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq,
     std::string const& table_id, std::string const& prefix) {
    future<google::cloud::Status> status_future =
        admin.AsyncDropRowsByPrefix(cq, table_id, prefix);
    auto final = status_future.then([prefix](future<google::cloud::Status> f) {
      auto status = f.get();
      if (!status.ok()) throw std::runtime_error(status.message());
      std::cout << "Successfully dropped rows with prefix " << prefix << "\n";
    });

    final.get();  // block to simplify example.
  }
  //! [async drop rows by prefix]
  (std::move(admin), std::move(cq), argv.at(0), argv.at(1));
}

void AsyncDropAllRows(google::cloud::bigtable::TableAdmin const& admin,
                      google::cloud::bigtable::CompletionQueue cq,
                      std::vector<std::string> const& argv) {
  //! [async drop all rows]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq,
     std::string const& table_id) {
    future<google::cloud::Status> status_future =
        admin.AsyncDropAllRows(cq, table_id);
    auto final =
        status_future.then([table_id](future<google::cloud::Status> f) {
          auto status = f.get();
          if (!status.ok()) throw std::runtime_error(status.message());
          std::cout << "Successfully dropped all rows for table_id " << table_id
                    << "\n";
        });
    final.get();  // block to simplify example.
  }
  //! [async drop all rows]
  (std::move(admin), std::move(cq), argv.at(0));
}

void AsyncCheckConsistency(google::cloud::bigtable::TableAdmin const& admin,
                           google::cloud::bigtable::CompletionQueue cq,
                           std::vector<std::string> const& argv) {
  //! [async check consistency]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq,
     std::string const& table_id, std::string const& consistency_token) {
    future<void> final =
        admin.AsyncCheckConsistency(cq, table_id, consistency_token)
            .then([consistency_token](future<StatusOr<cbt::Consistency>> f) {
              auto consistency = f.get();
              if (!consistency) {
                throw std::runtime_error(consistency.status().message());
              }
              if (*consistency == cbt::Consistency::kConsistent) {
                std::cout << "Table is consistent\n";
              } else {
                std::cout << "Table is not yet consistent, Please try again"
                          << " later with the same token (" << consistency_token
                          << ")\n";
              }
            });
    final.get();  // block to simplify example.
  }
  //! [async check consistency]
  (std::move(admin), std::move(cq), argv.at(0), argv.at(1));
}

void AsyncGenerateConsistencyToken(
    google::cloud::bigtable::TableAdmin const& admin,
    google::cloud::bigtable::CompletionQueue cq,
    std::vector<std::string> const& argv) {
  //! [async generate consistency token]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq,
     std::string const& table_id) {
    future<StatusOr<std::string>> token_future =
        admin.AsyncGenerateConsistencyToken(cq, table_id);
    auto final = token_future.then([](future<StatusOr<std::string>> f) {
      auto token = f.get();
      if (!token) throw std::runtime_error(token.status().message());
      std::cout << "generated token is : " << *token << "\n";
    });
    final.get();  // block to simplify example.
  }
  //! [async generate consistency token]
  (std::move(admin), std::move(cq), argv.at(0));
}

void AsyncWaitForConsistency(google::cloud::bigtable::TableAdmin const& admin,
                             google::cloud::bigtable::CompletionQueue cq,
                             std::vector<std::string> const& argv) {
  //! [async wait for consistency]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq,
     std::string const& table_id, std::string const& consistency_token) {
    future<StatusOr<cbt::Consistency>> result =
        admin.AsyncWaitForConsistency(cq, table_id, consistency_token);

    auto final = result.then([&](future<StatusOr<cbt::Consistency>> f) {
      auto consistent = f.get();
      if (!consistent) throw std::runtime_error(consistent.status().message());
      if (*consistent == cbt::Consistency::kConsistent) {
        std::cout << "The table " << table_id << " is now consistent with"
                  << " the token " << consistency_token << "\n";
      } else {
        std::cout << "Table is not yet consistent, Please try again"
                  << " later with the same token (" << consistency_token
                  << ")\n";
      }
    });
    final.get();  // block to simplify example.
  }
  //! [async wait for consistency]
  (std::move(admin), std::move(cq), argv.at(0), argv.at(1));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::bigtable::examples;
  namespace cbt = google::cloud::bigtable;

  if (!argv.empty()) throw Usage{"auto"};
  if (!examples::RunAdminIntegrationTests()) return;
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const instance_id = google::cloud::internal::GetEnv(
                               "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID")
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

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto table_id = examples::RandomTableId(prefix, generator);

  std::cout << "\nRunning the AsyncListTables() example [1]" << std::endl;
  AsyncListTables(admin, cq, {});

  std::cout << "\nRunning the AsyncCreateTable() example" << std::endl;
  AsyncCreateTable(admin, cq, {table_id});

  std::cout << "\nRunning the AsyncListTables() example [2]" << std::endl;
  AsyncListTables(admin, cq, {});

  std::cout << "\nRunning the AsyncGetTable() example" << std::endl;
  AsyncGetTable(admin, cq, {table_id});

  std::cout << "\nRunning the AsyncModifyTable() example" << std::endl;
  AsyncModifyTable(admin, cq, {table_id});

  std::cout << "\nRunning the AsyncGenerateConsistencyToken() example"
            << std::endl;
  AsyncGenerateConsistencyToken(admin, cq, {table_id});

  auto token = admin.GenerateConsistencyToken(table_id);
  if (!token) throw std::runtime_error(token.status().message());

  std::cout << "\nRunning the AsyncCheckConsistency() example" << std::endl;
  AsyncCheckConsistency(admin, cq, {table_id, *token});

  std::cout << "\nRunning the AsyncWaitForConsistency() example" << std::endl;
  AsyncWaitForConsistency(admin, cq, {table_id, *token});

  std::cout << "\nRunning the AsyncDropRowsByPrefix() example" << std::endl;
  AsyncDropRowsByPrefix(admin, cq, {table_id, "sample/prefix/"});

  std::cout << "\nRunning the AsyncDropAllRows() example" << std::endl;
  AsyncDropAllRows(admin, cq, {table_id});

  std::cout << "\nRunning the AsyncDeleteTable() example" << std::endl;
  AsyncDeleteTable(admin, cq, {table_id});
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InstallCrashHandler(argv[0]);

  namespace examples = google::cloud::bigtable::examples;
  examples::Example example({
      examples::MakeCommandEntry("async-create-table", {"<table-id>"},
                                 AsyncCreateTable),
      examples::MakeCommandEntry("async-list-tables", {}, AsyncListTables),
      examples::MakeCommandEntry("async-get-table", {"<table-id>"},
                                 AsyncGetTable),
      examples::MakeCommandEntry("async-delete-table", {"<table-id>"},
                                 AsyncDeleteTable),
      examples::MakeCommandEntry("async-modify-table", {"<table-id>"},
                                 AsyncModifyTable),
      examples::MakeCommandEntry("async-drop-rows-by-prefix",
                                 {"<table-id>", "<prefix>"},
                                 AsyncDropRowsByPrefix),
      examples::MakeCommandEntry("async-drop-all-rows", {"<table-id>"},
                                 AsyncDropAllRows),
      examples::MakeCommandEntry("async-check-consistency",
                                 {"<table-id>", "<consistency-token>"},
                                 AsyncCheckConsistency),
      examples::MakeCommandEntry("async-generate-consistency-token",
                                 {"<table-id>"}, AsyncGenerateConsistencyToken),
      examples::MakeCommandEntry("async-wait-for-consistency",
                                 {"<table-id>", "<consistency-token>"},
                                 AsyncWaitForConsistency),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
