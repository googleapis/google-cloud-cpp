// Copyright 2019 Google LLC
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

//! [all code]

//! [bigtable includes]
#include "google/cloud/bigtable/internal/table.h"
#include "google/cloud/bigtable/table.h"
//! [bigtable includes]
#include <google/protobuf/text_format.h>

namespace {
namespace cbt = google::cloud::bigtable;

struct Usage {
  std::string msg;
};

std::string command_usage;

void PrintUsage(std::string const& cmd, std::string const& msg) {
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << command_usage << std::endl;
}

void AsyncApply(cbt::Table table, cbt::CompletionQueue cq,
                std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-apply: <project-id> <instance-id> <table-id>"};
  }

  //! [async-apply]
  [&](cbt::Table table, cbt::CompletionQueue cq, std::string table_id) {
    // Write several rows with some trivial data.
    for (int i = 0; i != 20; ++i) {
      // Note: This example uses sequential numeric IDs for simplicity, but
      // this can result in poor performance in a production application.
      // Since rows are stored in sorted order by key, sequential keys can
      // result in poor distribution of operations across nodes.
      //
      // For more information about how to design a Bigtable schema for the
      // best performance, see the documentation:
      //
      //     https://cloud.google.com/bigtable/docs/schema-design

      char buf[32];
      snprintf(buf, sizeof(buf), "key-%06d", i);
      google::cloud::bigtable::SingleRowMutation mutation(buf);
      mutation.emplace_back(google::cloud::bigtable::SetCell(
          "fam", "col0", "value0-" + std::to_string(i)));
      mutation.emplace_back(google::cloud::bigtable::SetCell(
          "fam", "col1", "value2-" + std::to_string(i)));
      mutation.emplace_back(google::cloud::bigtable::SetCell(
          "fam", "col2", "value3-" + std::to_string(i)));
      mutation.emplace_back(google::cloud::bigtable::SetCell(
          "fam", "col3", "value4-" + std::to_string(i)));

      google::cloud::future<void> fut =
          table.AsyncApply(std::move(mutation), cq);
      fut.get();
    }
  }
  //! [async-apply]
  (std::move(table), std::move(cq), argv[1]);
}

void AsyncBulkApply(cbt::Table table, cbt::CompletionQueue cq,
                    std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-bulk-apply: <project-id> <instance-id> <table-id>"};
  }

  //! [bulk async-bulk-apply]
  [](cbt::Table table, cbt::CompletionQueue cq, std::string table_id) {
    // Write several rows in a single operation, each row has some trivial data.
    google::cloud::bigtable::BulkMutation bulk;
    for (int i = 0; i != 5000; ++i) {
      // Note: This example uses sequential numeric IDs for simplicity, but
      // this can result in poor performance in a production application.
      // Since rows are stored in sorted order by key, sequential keys can
      // result in poor distribution of operations across nodes.
      //
      // For more information about how to design a Bigtable schema for the
      // best performance, see the documentation:
      //
      //     https://cloud.google.com/bigtable/docs/schema-design
      char buf[32];
      snprintf(buf, sizeof(buf), "key-%06d", i);
      google::cloud::bigtable::SingleRowMutation mutation(buf);
      mutation.emplace_back(google::cloud::bigtable::SetCell(
          "fam", "col0", "value0-" + std::to_string(i)));
      mutation.emplace_back(google::cloud::bigtable::SetCell(
          "fam", "col1", "value2-" + std::to_string(i)));
      mutation.emplace_back(google::cloud::bigtable::SetCell(
          "fam", "col2", "value3-" + std::to_string(i)));
      mutation.emplace_back(google::cloud::bigtable::SetCell(
          "fam", "col3", "value4-" + std::to_string(i)));
      bulk.emplace_back(std::move(mutation));
    }

    google::cloud::future<void> fut = table.AsyncBulkApply(std::move(bulk), cq);

    fut.get();
  }
  //! [bulk async-bulk-apply]
  (std::move(table), std::move(cq), argv[1]);
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  using CommandType = std::function<void(
      google::cloud::bigtable::Table, google::cloud::bigtable::CompletionQueue,
      std::vector<std::string>)>;

  std::map<std::string, CommandType> commands = {
      {"async-apply", &AsyncApply}, {"async-bulk-apply", &AsyncBulkApply}};

  google::cloud::bigtable::CompletionQueue cq;

  {
    // Force each command to generate its Usage string, so we can provide a good
    // usage string for the whole program. We need to create an Table
    // object to do this, but that object is never used, it is passed to the
    // commands, without any calls made to it.
    google::cloud::bigtable::Table unused(
        google::cloud::bigtable::CreateDefaultDataClient(
            "unused-project", "Unused-instance",
            google::cloud::bigtable::ClientOptions()),
        "Unused-table");
    for (auto&& kv : commands) {
      try {
        kv.second(unused, cq, {});
      } catch (Usage const& u) {
        command_usage += "    ";
        command_usage += u.msg;
        command_usage += "\n";
      }
    }
  }

  if (argc < 5) {
    PrintUsage(argv[0],
               "Missing command and/or project-id/ or instance-id or table-id");
    return 1;
  }

  std::vector<std::string> args;
  args.emplace_back(argv[0]);
  std::string const command_name = argv[1];
  std::string const project_id = argv[2];
  std::string const instance_id = argv[3];
  std::string const table_id = argv[4];
  std::transform(argv + 4, argv + argc, std::back_inserter(args),
                 [](char* x) { return std::string(x); });

  auto command = commands.find(command_name);
  if (commands.end() == command) {
    PrintUsage(argv[0], "Unknown command: " + command_name);
    return 1;
  }

  // Start a thread to run the completion queue event loop.
  std::thread runner([&cq] { cq.Run(); });

  // Connect to the Cloud Bigtable endpoint.
  //! [connect data]
  google::cloud::bigtable::Table table(
      google::cloud::bigtable::CreateDefaultDataClient(
          project_id, instance_id, google::cloud::bigtable::ClientOptions()),
      table_id);
  //! [connect data]

  command->second(table, cq, args);

  // Shutdown the completion queue event loop and join the thread.
  cq.Shutdown();
  runner.join();

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argv[0], ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
//! [all code]
