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

void AsyncGetIamPolicy(google::cloud::bigtable::TableAdmin admin,
                       google::cloud::bigtable::CompletionQueue cq,
                       std::vector<std::string> argv) {
  //! [async get iam policy]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  using google::iam::v1::Policy;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq, std::string table_id) {
    future<StatusOr<google::iam::v1::Policy>> policy_future =
        admin.AsyncGetIamPolicy(cq, table_id);

    future<void> final =
        policy_future.then([](future<StatusOr<google::iam::v1::Policy>> f) {
          StatusOr<google::iam::v1::Policy> iam = f.get();
          if (!iam) throw std::runtime_error(iam.status().message());
          std::cout << "IamPolicy details : " << iam->DebugString() << "\n";
        });

    final.get();  // block to keep the example simple
  }
  //! [async get iam policy]
  (std::move(admin), std::move(cq), argv.at(0));
}

void AsyncSetIamPolicy(google::cloud::bigtable::TableAdmin admin,
                       google::cloud::bigtable::CompletionQueue cq,
                       std::vector<std::string> argv) {
  //! [async set iam policy]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  using google::iam::v1::Policy;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq, std::string table_id,
     std::string role, std::string member) {
    future<StatusOr<google::iam::v1::Policy>> updated_future =
        admin.AsyncGetIamPolicy(cq, table_id)
            .then([cq, admin, role, member,
                   table_id](future<StatusOr<google::iam::v1::Policy>>
                                 current_future) mutable {
              auto current = current_future.get();
              if (!current) {
                return google::cloud::make_ready_future<
                    StatusOr<google::iam::v1::Policy>>(current.status());
              }
              // This example adds the member to all existing bindings for
              // that role. If there are no such bindgs, it adds a new one.
              // This might not be what the user wants, e.g. in case of
              // conditional bindings.
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
              return admin.AsyncSetIamPolicy(cq, table_id, *current);
            });
    // Show how to perform additional work while the long running operation
    // completes. The application could use future.then() instead.
    std::cout << "Waiting for IAM policy update to complete " << std::flush;
    updated_future.wait_for(std::chrono::seconds(2));
    auto result = updated_future.get();
    std::cout << '.' << std::flush;
    if (!result) throw std::runtime_error(result.status().message());
    using cbt::operator<<;
    std::cout << "DONE, the IAM Policy for " << table_id << " is\n"
              << *result << "\n";
  }
  //! [async set iam policy]
  (std::move(admin), std::move(cq), argv.at(0), argv.at(1), argv.at(2));
}

void AsyncTestIamPermissions(google::cloud::bigtable::TableAdmin admin,
                             google::cloud::bigtable::CompletionQueue cq,
                             std::vector<std::string> argv) {
  //! [async test iam permissions]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq, std::string resource,
     std::vector<std::string> permissions) {
    future<StatusOr<std::vector<std::string>>> permissions_future =
        admin.AsyncTestIamPermissions(cq, resource, permissions);
    // Show how to perform additional work while the long running operation
    // completes. The application could use permissions_future.then() instead.
    std::cout << "Waiting for TestIamPermissions " << std::flush;
    permissions_future.wait_for(std::chrono::seconds(2));
    std::cout << '.' << std::flush;
    auto result = permissions_future.get();
    if (!result) throw std::runtime_error(result.status().message());
    std::cout << "DONE, the current user has the following permissions [";
    char const* sep = "";
    for (auto const& p : *result) {
      std::cout << sep << p;
      sep = ", ";
    }
    std::cout << "]\n";
  }
  //! [async test iam permissions]
  (std::move(admin), std::move(cq), argv.at(0), {argv.begin() + 1, argv.end()});
}

void AsyncTestIamPermissionsCommand(std::vector<std::string> argv) {
  if (argv.size() < 3) {
    throw Usage{
        "async-test-iam-permissions <project-id> <instance-id> <resource-id>"
        "<permission> [permission ...]"};
  }
  auto const project_id = argv[0];
  auto const instance_id = argv[1];
  argv.erase(argv.begin(), argv.begin() + 2);

  google::cloud::CompletionQueue cq;
  std::thread th([&cq] { cq.Run(); });
  google::cloud::bigtable::examples::AutoShutdownCQ shutdown(cq, std::move(th));

  google::cloud::bigtable::TableAdmin admin(
      google::cloud::bigtable::CreateDefaultAdminClient(
          project_id, google::cloud::bigtable::ClientOptions{}),
      instance_id);

  AsyncTestIamPermissions(admin, cq, std::move(argv));
}

}  // anonymous namespace

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
