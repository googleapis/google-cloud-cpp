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

#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"
#include <google/protobuf/text_format.h>
#include <algorithm>

namespace {
struct Usage {
  std::string msg;
};

std::string command_usage;

void PrintUsage(std::string const& cmd, std::string const& msg) {
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << command_usage << "\n";
}

void AsyncCreateTable(google::cloud::bigtable::TableAdmin admin,
                      google::cloud::bigtable::CompletionQueue cq,
                      std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-create-table <project-id> <instance-id> <table-id>"};
  }

  //! [async create table]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq, std::string table_id) {
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
          if (!table) {
            throw std::runtime_error(table.status().message());
          }
          std::cout << "Table created as " << table->name() << "\n";
          return google::cloud::Status();
        });
    final.get();  // block to simplify the example.
  }
  //! [async create table]
  (std::move(admin), std::move(cq), argv[1]);
}

void AsyncListTables(google::cloud::bigtable::TableAdmin admin,
                     google::cloud::bigtable::CompletionQueue cq,
                     std::vector<std::string> argv) {
  if (argv.size() != 1U) {
    throw Usage{"async-list-tables <project-id> <instance-id>"};
  }

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
          if (!tables) {
            throw std::runtime_error(tables.status().message());
          }
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

void AsyncGetTable(google::cloud::bigtable::TableAdmin admin,
                   google::cloud::bigtable::CompletionQueue cq,
                   std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-get-table <project-id> <instance-id> <table-id>"};
  }

  //! [async get table]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq, std::string table_id) {
    future<StatusOr<google::bigtable::admin::v2::Table>> table_future =
        admin.AsyncGetTable(cq, table_id,
                            google::bigtable::admin::v2::Table::FULL);

    auto final = table_future.then(
        [](future<StatusOr<google::bigtable::admin::v2::Table>> f) {
          auto table = f.get();
          if (!table) {
            throw std::runtime_error(table.status().message());
          }
          std::cout << table->name() << "\n";
          for (auto const& family : table->column_families()) {
            std::string const& family_name = family.first;
            std::string gc_rule;
            google::protobuf::TextFormat::PrintToString(family.second.gc_rule(),
                                                        &gc_rule);
            std::cout << "\t" << family_name << "\t\t" << gc_rule << "\n";
          }
          return google::cloud::Status();
        });

    final.get();  // block to simplify the example.
  }
  //! [async get table]
  (std::move(admin), std::move(cq), argv[1]);
}

void AsyncDeleteTable(google::cloud::bigtable::TableAdmin admin,
                      google::cloud::bigtable::CompletionQueue cq,
                      std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-delete-table <project-id> <instance-id> <table-id>"};
  }

  //! [async delete table]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq, std::string table_id) {
    future<google::cloud::Status> status_future =
        admin.AsyncDeleteTable(cq, table_id);

    auto final =
        status_future.then([table_id](future<google::cloud::Status> f) {
          auto status = f.get();
          if (!status.ok()) {
            throw std::runtime_error(status.message());
          }
          std::cout << "Successfully deleted table: " << table_id << "\n";
        });

    final.get();  // block to simplify example.
  }
  //! [async delete table]
  (std::move(admin), std::move(cq), argv[1]);
}

void AsyncModifyTable(google::cloud::bigtable::TableAdmin admin,
                      google::cloud::bigtable::CompletionQueue cq,
                      std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-modify-table <project-id> <instance-id> <table-id>"};
  }

  //! [async modify table]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq, std::string table_id) {
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
          if (!table) {
            throw std::runtime_error(table.status().message());
          }
          std::cout << table->name() << ":\n";
          std::cout << table->DebugString() << "\n";
        });

    final.get();  // block to simplify example.
  }
  //! [async modify table]
  (std::move(admin), std::move(cq), argv[1]);
}

void AsyncDropRowsByPrefix(google::cloud::bigtable::TableAdmin admin,
                           google::cloud::bigtable::CompletionQueue cq,
                           std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{
        "async-drop-rows-by-prefix <project-id> <instance-id> <table-id> "
        "<row-key>"};
  }

  //! [async drop rows by prefix]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq, std::string table_id,
     std::string row_key) {
    future<google::cloud::Status> status_future =
        admin.AsyncDropRowsByPrefix(cq, table_id, row_key);
    auto final = status_future.then([row_key](future<google::cloud::Status> f) {
      auto status = f.get();
      if (!status.ok()) {
        throw std::runtime_error(status.message());
      }
      std::cout << "Successfully dropped rows with prefix " << row_key << "\n";
    });

    final.get();  // block to simplify example.
  }
  //! [async drop rows by prefix]
  (std::move(admin), std::move(cq), argv[1], argv[2]);
}

void AsyncDropAllRows(google::cloud::bigtable::TableAdmin admin,
                      google::cloud::bigtable::CompletionQueue cq,
                      std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-drop-all-rows <project-id> <instance-id> <table-id>"};
  }

  //! [async drop all rows]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq, std::string table_id) {
    future<google::cloud::Status> status_future =
        admin.AsyncDropAllRows(cq, table_id);
    auto final =
        status_future.then([table_id](future<google::cloud::Status> f) {
          auto status = f.get();
          if (!status.ok()) {
            throw std::runtime_error(status.message());
          }
          std::cout << "Successfully dropped all rows for table_id " << table_id
                    << "\n";
        });
    final.get();  // block to simplify example.
  }
  //! [async drop all rows]
  (std::move(admin), std::move(cq), argv[1]);
}

void AsyncCheckConsistency(google::cloud::bigtable::TableAdmin admin,
                           google::cloud::bigtable::CompletionQueue cq,
                           std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{
        "async-check-consistency <project-id> <instance-id> <table-id> "
        "<consistency_token>"};
  }

  //! [async check consistency]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq, std::string table_id,
     std::string consistency_token) {
    future<void> final =
        admin
            .AsyncCheckConsistency(cq, cbt::TableId(table_id),
                                   cbt::ConsistencyToken(consistency_token))
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
  (std::move(admin), std::move(cq), argv[1], argv[2]);
}

void AsyncGenerateConsistencyToken(google::cloud::bigtable::TableAdmin admin,
                                   google::cloud::bigtable::CompletionQueue cq,
                                   std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{
        "async-generate-consistency-token <project-id> <instance-id> "
        "<table-id>"};
  }

  //! [async generate consistency token]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq, std::string table_id) {
    future<StatusOr<cbt::ConsistencyToken>> token_future =
        admin.AsyncGenerateConsistencyToken(cq, table_id);
    auto final =
        token_future.then([](future<StatusOr<cbt::ConsistencyToken>> f) {
          auto token = f.get();
          if (!token) {
            throw std::runtime_error(token.status().message());
          }
          std::cout << "generated token is : " << token->get() << "\n";
        });
    final.get();  // block to simplify example.
  }
  //! [async generate consistency token]
  (std::move(admin), std::move(cq), argv[1]);
}
}  // anonymous namespace

int main(int argc, char* argv[]) try {
  using CommandType = std::function<void(
      google::cloud::bigtable::TableAdmin,
      google::cloud::bigtable::CompletionQueue, std::vector<std::string>)>;

  std::map<std::string, CommandType> commands = {
      {"async-create-table", &AsyncCreateTable},
      {"async-list-tables", &AsyncListTables},
      {"async-get-table", &AsyncGetTable},
      {"async-delete-table", &AsyncDeleteTable},
      {"async-modify-table", &AsyncModifyTable},
      {"async-drop-rows-by-prefix", &AsyncDropRowsByPrefix},
      {"async-drop-all-rows", &AsyncDropAllRows},
      {"async-check-consistency", &AsyncCheckConsistency},
      {"async-generate-consistency-token", &AsyncGenerateConsistencyToken},
  };

  google::cloud::bigtable::CompletionQueue cq;

  {
    // Force each command to generate its Usage string, so we can provide a good
    // usage string for the whole program. We need to create an TableAdmin
    // object to do this, but that object is never used, it is passed to the
    // commands, without any calls made to it.
    google::cloud::bigtable::TableAdmin unused(
        google::cloud::bigtable::CreateDefaultAdminClient(
            "unused-project", google::cloud::bigtable::ClientOptions()),
        "Unused-instance");
    for (auto&& kv : commands) {
      try {
        kv.second(unused, cq, {});
      } catch (Usage const& u) {
        command_usage += "    ";
        command_usage += u.msg;
        command_usage += "\n";
      } catch (...) {
        // ignore other exceptions.
      }
    }
  }

  if (argc < 4) {
    PrintUsage(argv[0], "Missing command and/or project-id/ or instance-id");
    return 1;
  }

  std::vector<std::string> args;
  args.emplace_back(argv[0]);
  std::string const command_name = argv[1];
  std::string const project_id = argv[2];
  std::string const instance_id = argv[3];
  std::transform(argv + 4, argv + argc, std::back_inserter(args),
                 [](char* x) { return std::string(x); });

  auto command = commands.find(command_name);
  if (commands.end() == command) {
    PrintUsage(argv[0], "Unknown command: " + command_name);
    return 1;
  }

  // Start a thread to run the completion queue event loop.
  std::thread runner([&cq] { cq.Run(); });

  // Connect to the Cloud Bigtable admin endpoint.
  google::cloud::bigtable::TableAdmin admin(
      google::cloud::bigtable::CreateDefaultAdminClient(
          project_id, google::cloud::bigtable::ClientOptions()),
      instance_id);

  command->second(admin, cq, args);

  // Shutdown the completion queue event loop and join the thread.
  cq.Shutdown();
  runner.join();

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argv[0], ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}
