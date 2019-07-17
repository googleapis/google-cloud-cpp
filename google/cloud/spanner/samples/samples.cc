// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include <sstream>
#include <tuple>
#include <utility>

namespace {

std::string RandomDatabaseName(
    google::cloud::internal::DefaultPRNG& generator) {
  // A database ID must be between 2 and 30 characters, fitting the regular
  // expression `[a-z][a-z0-9_\-]*[a-z0-9]`
  int max_size = 30;
  std::string const prefix = "db-";
  auto size = static_cast<int>(max_size - 1 - prefix.size());
  return prefix +
         google::cloud::internal::Sample(
             generator, size, "abcdefghijlkmnopqrstuvwxyz012345689_-") +
         google::cloud::internal::Sample(generator, 1,
                                         "abcdefghijlkmnopqrstuvwxyz");
}

void CreateDatabase(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "create-database <project-id> <instance-id> <database-id>");
  }

  //! [create-database] [START spanner_create_database]
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](std::string const& project_id, std::string const& instance_id,
     std::string const& database_id) {
    google::cloud::spanner::DatabaseAdminClient client;
    future<StatusOr<google::spanner::admin::database::v1::Database>> future =
        client.CreateDatabase(project_id, instance_id, database_id,
                              {R"""(
                        CREATE TABLE Singers (
                                SingerId   INT64 NOT NULL,
                                FirstName  STRING(1024),
                                LastName   STRING(1024),
                                SingerInfo BYTES(MAX)
                        ) PRIMARY KEY (SingerId))""",
                               R"""(CREATE TABLE Albums (
                                SingerId     INT64 NOT NULL,
                                AlbumId      INT64 NOT NULL,
                                AlbumTitle   STRING(MAX)
                        ) PRIMARY KEY (SingerId, AlbumId),
                        INTERLEAVE IN PARENT Singers ON DELETE CASCADE)"""});
    StatusOr<google::spanner::admin::database::v1::Database> db = future.get();
    if (!db) {
      throw std::runtime_error(db.status().message());
    }
    std::cout << "Created database [" << database_id << "]\n";
  }
  //! [create-database] [END spanner_create_database]
  (argv[0], argv[1], argv[2]);
}

void AddColumn(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "add-column <project-id> <instance-id> <database-id>");
  }

  //! [update-database] [START spanner_add_column]
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](std::string const& project_id, std::string const& instance_id,
     std::string const& database_id) {
    google::cloud::spanner::DatabaseAdminClient client;
    future<StatusOr<
        google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>>
        future = client.UpdateDatabase(
            project_id, instance_id, database_id,
            {"ALTER TABLE Albums ADD COLUMN MarketingBudget INT64"});
    StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>
        metadata = future.get();
    if (!metadata) {
      throw std::runtime_error(metadata.status().message());
    }
    std::cout << "Added MarketingBudget column\n";
  }
  //! [update-database] [END spanner_add_column]
  (argv[0], argv[1], argv[2]);
}

void QueryWithStruct(std::vector<std::string> const&) {
  // TODO(#188): Add querying part once data client is ready.
  // [START spanner_create_struct_with_data]
  auto singer_info = std::make_tuple(std::make_pair("FirstName", "Elena"),
                                     std::make_pair("LastName", "Campbell"));
  // [END spanner_create_struct_with_data]
  std::cout << "Struct created with the following data:\n"
            << std::get<0>(singer_info).first << ":"
            << std::get<0>(singer_info).second << "\n"
            << std::get<1>(singer_info).first << ":"
            << std::get<1>(singer_info).second << "\n";
}

void DropDatabase(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "drop-database <project-id> <instance-id> <database-id>");
  }

  //! [drop-database] [START spanner_drop_database]
  [](std::string const& project_id, std::string const& instance_id,
     std::string const& database_id) {
    google::cloud::spanner::DatabaseAdminClient client;
    google::cloud::Status status =
        client.DropDatabase(project_id, instance_id, database_id);
    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
    std::cout << "Database " << database_id << " successfully dropped\n";
  }
  //! [drop-database] [END spanner_drop_database]
  (argv[0], argv[1], argv[2]);
}

int RunOneCommand(std::vector<std::string> argv) {
  using CommandType = std::function<void(std::vector<std::string> const&)>;

  std::map<std::string, CommandType> commands = {
      {"create-database", &CreateDatabase},
      {"add-column", &AddColumn},
      {"query-with-struct", &QueryWithStruct},
      {"drop-database", &DropDatabase},
  };

  std::string usage_msg = [&argv, &commands] {
    auto last_slash = std::string(argv[0]).find_last_of("/\\");
    auto program = argv[0].substr(last_slash + 1);
    std::string usage;
    usage += "Usage: ";
    usage += program;
    usage += " <command> [arguments]\n\n";
    usage += "Commands:\n";
    for (auto&& kv : commands) {
      try {
        kv.second({});
      } catch (std::exception const& ex) {
        usage += "    ";
        usage += ex.what();
        usage += "\n";
      }
    }
    return usage;
  }();

  if (argv.size() < 2) {
    std::cerr << "Missing command argument\n" << usage_msg << "\n";
    return 1;
  }
  std::string command_name = argv[1];
  argv.erase(argv.begin());  // remove the program name from the list.
  argv.erase(argv.begin());  // remove the command name from the list.

  auto command = commands.find(command_name);
  if (commands.end() == command) {
    std::cerr << "Unknown command " << command_name << "\n"
              << usage_msg << "\n";
    return 1;
  }

  // Run the command.
  command->second(argv);
  return 0;
}

void RunAll() {
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  auto instance_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_SPANNER_INSTANCE")
          .value_or("");

  if (project_id.empty()) {
    throw std::runtime_error("GOOGLE_CLOUD_PROJECT is not set or is empty");
  }
  if (instance_id.empty()) {
    throw std::runtime_error(
        "GOOGLE_CLOUD_CPP_SPANNER_INSTANCE is not set or is empty");
  }
  auto generator = google::cloud::internal::MakeDefaultPRNG();
  std::string database_id = RandomDatabaseName(generator);

  RunOneCommand({"", "create-database", project_id, instance_id, database_id});
  RunOneCommand({"", "add-column", project_id, instance_id, database_id});
  RunOneCommand(
      {"", "query-with-struct", project_id, instance_id, database_id});
  RunOneCommand({"", "drop-database", project_id, instance_id, database_id});
}

bool AutoRun() {
  return google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES")
             .value_or("") == "yes";
}

}  // namespace

int main(int ac, char* av[]) try {
  if (AutoRun()) {
    RunAll();
    return 0;
  }

  return RunOneCommand({av, av + ac});
} catch (std::exception const& ex) {
  std::cerr << ex.what() << "\n";
  return 1;
}
