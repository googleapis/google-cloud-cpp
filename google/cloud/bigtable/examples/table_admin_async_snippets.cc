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

namespace {
namespace cbt = google::cloud::bigtable;

struct Usage {
  std::string msg;
};

char const* ConsumeArg(int& argc, char* argv[]) {
  if (argc < 2) {
    return nullptr;
  }
  char const* result = argv[1];
  std::copy(argv + 2, argv + argc, argv + 1);
  argc--;
  return result;
}

std::string command_usage;

void PrintUsage(int argc, char* argv[], std::string const& msg) {
  std::string const cmd = argv[0];
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << command_usage << std::endl;
}

void AsyncCreateTable(cbt::TableAdmin admin, cbt::CompletionQueue cq, int argc,
                      char* argv[]) {
  if (argc != 2) {
    throw Usage{"async-create-table: <project-id> <instance-id> <table-id>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);

  //! [async create table]
  namespace cbt = google::cloud::bigtable;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq, std::string table_id) {
    google::cloud::future<google::bigtable::admin::v2::Table> future =
        admin.AsyncCreateTable(
            table_id,
            google::cloud::bigtable::TableConfig(
                {{"fam", google::cloud::bigtable::GcRule::MaxNumVersions(10)},
                 {"foo", google::cloud::bigtable::GcRule::MaxAge(
                             std::chrono::hours(72))}},
                {}),
            cq);

    auto final = future.then(
        [](google::cloud::future<google::bigtable::admin::v2::Table> f) {
          auto table = f.get();
          std::cout << "Table created as " << table.name() << std::endl;
        });
    final.get();  // block to keep sample small and correct.
  }
  //! [async create table]
  (std::move(admin), std::move(cq), table_id);
}

void AsyncGetTable(cbt::TableAdmin admin, cbt::CompletionQueue cq, int argc,
                   char* argv[]) {
  if (argc != 2) {
    throw Usage{"async-get-table: <project-id> <instance-id> <table-id>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);

  //! [async get table]
  namespace cbt = google::cloud::bigtable;
  [](cbt::TableAdmin admin, cbt::CompletionQueue cq, std::string table_id) {
    google::cloud::future<google::bigtable::admin::v2::Table> future =
        admin.AsyncGetTable(table_id, google::bigtable::admin::v2::Table::FULL,
                            cq);

    auto final = future.then(
        [](google::cloud::future<google::bigtable::admin::v2::Table> f) {
          auto table = f.get();
          std::cout << table.name() << "\n";
          for (auto const& family : table.column_families()) {
            std::string const& family_name = family.first;
            std::string gc_rule;
            google::protobuf::TextFormat::PrintToString(family.second.gc_rule(),
                                                        &gc_rule);
            std::cout << "\t" << family_name << "\t\t" << gc_rule << std::endl;
          }
        });

    final.get();
  }
  //! [async get table]
  (std::move(admin), std::move(cq), table_id);
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  using CommandType = std::function<void(
      google::cloud::bigtable::TableAdmin,
      google::cloud::bigtable::CompletionQueue, int, char*[])>;

  std::map<std::string, CommandType> commands = {
      {"async-create-table", &AsyncCreateTable},
      {"async-get-table", &AsyncGetTable},
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
        int fake_argc = 0;
        kv.second(unused, cq, fake_argc, argv);
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
    PrintUsage(argc, argv, "Missing command and/or project-id/ or instance-id");
    return 1;
  }

  std::string const command_name = ConsumeArg(argc, argv);
  std::string const project_id = ConsumeArg(argc, argv);
  std::string const instance_id = ConsumeArg(argc, argv);

  auto command = commands.find(command_name);
  if (commands.end() == command) {
    PrintUsage(argc, argv, "Unknown command: " + command_name);
    return 1;
  }

  // Start a thread to run the completion queue event loop.
  std::thread runner([&cq] { cq.Run(); });

  // Connect to the Cloud Bigtable admin endpoint.
  google::cloud::bigtable::TableAdmin admin(
      google::cloud::bigtable::CreateDefaultAdminClient(
          project_id, google::cloud::bigtable::ClientOptions()),
      instance_id);

  command->second(admin, cq, argc, argv);

  // Shutdown the completion queue event loop and join the thread.
  cq.Shutdown();
  runner.join();

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argc, argv, ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
