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

#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/status.h"
#include <google/longrunning/operations.grpc.pb.h>
#include <google/spanner/admin/database/v1/spanner_database_admin.grpc.pb.h>
#include <google/spanner/v1/spanner.grpc.pb.h>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>

namespace {

int ListDatabases(std::vector<std::string> args) {
  if (args.size() != 2U) {
    std::cerr << "list-databases <project> <instance>\n";
    return 1;
  }
  auto const& project = args[0];
  auto const& instance = args[1];

  namespace spanner = google::spanner::admin::database::v1;

  std::shared_ptr<grpc::ChannelCredentials> cred =
      grpc::GoogleDefaultCredentials();
  std::shared_ptr<grpc::Channel> channel =
      grpc::CreateChannel("spanner.googleapis.com", std::move(cred));
  std::unique_ptr<spanner::DatabaseAdmin::Stub> stub(
      spanner::DatabaseAdmin::NewStub(std::move(channel)));

  spanner::ListDatabasesResponse response;
  spanner::ListDatabasesRequest request;
  request.set_parent("projects/" + project + "/instances/" + instance);

  grpc::ClientContext context;
  grpc::Status status = stub->ListDatabases(&context, request, &response);

  if (!status.ok()) {
    std::cerr << "FAILED: " << status.error_code() << ": "
              << status.error_message() << "\n";
    return 1;
  }

  std::cout << "Response:\n";
  std::cout << response.DebugString() << "\n";
  return 0;
}

int WaitForOperation(std::shared_ptr<grpc::Channel> channel,
                     google::longrunning::Operation operation) {
  auto stub = google::longrunning::Operations::NewStub(std::move(channel));

  std::cout << "Waiting for operation " << operation.name() << " "
            << std::flush;
  while (!operation.done()) {
    // Spanner operations can take minutes, but in small experiments like these
    // they typically take a few seconds.
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << '.' << std::flush;
    grpc::ClientContext context;
    google::longrunning::GetOperationRequest request;
    request.set_name(operation.name());
    google::longrunning::Operation update;
    grpc::Status status = stub->GetOperation(&context, request, &update);
    if (!status.ok()) {
      std::cerr << __func__ << " FAILED: " << status.error_code() << ": "
                << status.error_message() << "\n";
      return 1;
    }
    operation.Swap(&update);
  }
  std::cout << " DONE\n" << operation.DebugString() << "\n";
  return 0;
}

int CreateDatabase(std::vector<std::string> args) {
  if (args.size() != 3U) {
    std::cerr << "create-database <project> <instance> <database>\n";
    return 1;
  }
  auto const& project = args[0];
  auto const& instance = args[1];
  auto const& database = args[2];

  namespace spanner = google::spanner::admin::database::v1;

  std::shared_ptr<grpc::ChannelCredentials> cred =
      grpc::GoogleDefaultCredentials();
  std::shared_ptr<grpc::Channel> channel =
      grpc::CreateChannel("spanner.googleapis.com", cred);
  auto stub = spanner::DatabaseAdmin::NewStub(channel);

  spanner::CreateDatabaseRequest request;
  google::longrunning::Operation operation;
  request.set_parent("projects/" + project + "/instances/" + instance);
  request.set_create_statement("CREATE DATABASE `" + database + "`");

  grpc::ClientContext context;
  grpc::Status status = stub->CreateDatabase(&context, request, &operation);

  if (!status.ok()) {
    std::cerr << "FAILED: " << status.error_code() << ": "
              << status.error_message() << "\n";
    return 1;
  }

  std::cout << "Response:\n";
  std::cout << operation.DebugString() << "\n";

  return WaitForOperation(std::move(channel), std::move(operation));
}

int DropDatabase(std::vector<std::string> args) {
  if (args.size() != 3U) {
    std::cerr << "drop-database <project> <instance> <database>\n";
    return 1;
  }
  auto const& project = args[0];
  auto const& instance = args[1];
  auto const& database = args[2];

  namespace spanner = google::spanner::admin::database::v1;

  std::shared_ptr<grpc::ChannelCredentials> cred =
      grpc::GoogleDefaultCredentials();
  std::shared_ptr<grpc::Channel> channel =
      grpc::CreateChannel("spanner.googleapis.com", cred);
  auto stub = spanner::DatabaseAdmin::NewStub(channel);

  spanner::DropDatabaseRequest request;
  google::protobuf::Empty response;
  request.set_database("projects/" + project + "/instances/" + instance +
                       "/databases/" + database);

  grpc::ClientContext context;
  grpc::Status status = stub->DropDatabase(&context, request, &response);

  if (!status.ok()) {
    std::cerr << "FAILED: " << status.error_code() << ": "
              << status.error_message() << "\n";
    return 1;
  }

  return 0;
}

int CreateTimeseriesTable(std::vector<std::string> args) {
  if (args.size() != 3U) {
    std::cerr << "create-timeseries-table <project> <instance> <database>\n";
    return 1;
  }
  auto const& project = args[0];
  auto const& instance = args[1];
  auto const& database = args[2];

  namespace spanner = google::spanner::admin::database::v1;

  std::shared_ptr<grpc::ChannelCredentials> cred =
      grpc::GoogleDefaultCredentials();
  std::shared_ptr<grpc::Channel> channel =
      grpc::CreateChannel("spanner.googleapis.com", cred);
  auto stub = spanner::DatabaseAdmin::NewStub(channel);

  spanner::UpdateDatabaseDdlRequest request;
  google::longrunning::Operation operation;
  request.set_database("projects/" + project + "/instances/" + instance +
                       "/databases/" + database);
  request.add_statements(R"""(
CREATE TABLE timeseries (
	name STRING(MAX) NOT NULL,
	ts TIMESTAMP NOT NULL,
	value INT64 NOT NULL,
) PRIMARY KEY (name, ts)
)""");

  grpc::ClientContext context;
  grpc::Status status = stub->UpdateDatabaseDdl(&context, request, &operation);

  if (!status.ok()) {
    std::cerr << "FAILED: " << status.error_code() << ": "
              << status.error_message() << "\n";
    return 1;
  }

  std::cout << "Response:\n";
  std::cout << operation.DebugString() << "\n";

  return WaitForOperation(std::move(channel), std::move(operation));
}

int PopulateTimeseriesTable(std::vector<std::string> args) {
  if (args.size() != 3U) {
    std::cerr << "populate-timeseries <project> <instance> <database>\n";
    return 1;
  }
  auto const& project = args[0];
  auto const& instance = args[1];
  auto const& database = args[2];

  std::string database_name = "projects/" + project + "/instances/" + instance +
                              "/databases/" + database;

  namespace spanner = google::spanner::v1;

  std::shared_ptr<grpc::ChannelCredentials> cred =
      grpc::GoogleDefaultCredentials();
  std::shared_ptr<grpc::Channel> channel =
      grpc::CreateChannel("spanner.googleapis.com", cred);
  auto stub = spanner::Spanner::NewStub(std::move(channel));

  spanner::Session session;
  {
    spanner::CreateSessionRequest request;
    request.set_database(database_name);

    grpc::ClientContext context;
    grpc::Status status = stub->CreateSession(&context, request, &session);
    if (!status.ok()) {
      std::cerr << "FAILED: [" << status.error_code() << "] - "
                << status.error_message() << "\n"
                << request.DebugString() << "\n";
      return 1;
    }
  }

  std::cout << "Session: " << session.name() << "\n";

  spanner::Transaction read_write_transaction;
  {
    spanner::BeginTransactionRequest request;
    request.set_session(session.name());
    *request.mutable_options()->mutable_read_write() = {};

    grpc::ClientContext context;
    grpc::Status status =
        stub->BeginTransaction(&context, request, &read_write_transaction);
    if (!status.ok()) {
      std::cerr << "FAILED: [" << status.error_code() << "] - "
                << status.error_message() << "\n";
      return 1;
    }
  }

  std::cout << "Transaction: " << read_write_transaction.id() << "\n";

  std::int64_t seqno = 0;
  auto insert_one = [&](std::string series_name,
                        std::chrono::system_clock::time_point ts,
                        std::int64_t value) {
    spanner::ExecuteSqlRequest request;
    request.set_session(session.name());
    request.mutable_transaction()->set_id(read_write_transaction.id());
    request.set_seqno(seqno++);
    request.set_sql(
        "INSERT INTO timeseries (name, ts, value)"
        " VALUES (@name, @time, @value)");
    auto& fields = *request.mutable_params()->mutable_fields();
    fields["name"].set_string_value(std::move(series_name));
    fields["time"].set_string_value(google::cloud::internal::FormatRfc3339(ts));
    fields["value"].set_string_value(std::to_string(value));
    auto& types = *request.mutable_param_types();
    types["time"].set_code(spanner::TIMESTAMP);

    grpc::ClientContext context;
    spanner::ResultSet result;
    grpc::Status status = stub->ExecuteSql(&context, request, &result);
    if (!status.ok()) {
      std::cerr << "INSERT INTO FAILED: [" << status.error_code() << "] - "
                << status.error_message() << "\n";
      return google::cloud::Status(google::cloud::StatusCode::kUnknown,
                                   status.error_message());
    }

    std::cout << "INSERT = " << result.DebugString() << "\n";
    return google::cloud::Status();
  };

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> d(0, 100);
  auto now = std::chrono::system_clock::now();

  for (int i = 0; i != 100; ++i) {
    std::string series_name = "celsius-temp-" + std::to_string(i);
    for (int j = 0; j != 100; ++j) {
      auto ts = now + std::chrono::seconds(j);
      insert_one(series_name, ts, d(gen));
    }
  }

  grpc::ClientContext context;
  spanner::CommitRequest request;
  spanner::CommitResponse response;
  request.set_session(session.name());
  request.set_transaction_id(read_write_transaction.id());
  grpc::Status commit_status = stub->Commit(&context, request, &response);
  if (!commit_status.ok()) {
    std::cerr << "COMMIT FAILED: [" << commit_status.error_code() << "] - "
              << commit_status.error_message() << "\n";
    return 1;
  }
  std::cout << "COMMIT = " << response.DebugString() << "\n";

  return 0;
}

}  // namespace

/**
 * @file
 *
 * This is a command-line tool to let folks easily experiment with Spanner
 * using C++. First, enable the spanner API in your project and create a
 * Cloud Spanner instance:
 *
 * @code
 * $ PROJECT_ID=...  # e.g. jgm-cloud-cxx
 * $ INSTANCE_ID=... # e.g. test-spanner-instance
 * $ gcloud services enable spanner.googleapis.com
 * $ gcloud spanner instances create ${INSTANCE_ID}
 *     --config=regional-us-central1 --description="${INSTANCE_ID}" --nodes=1
 * @endcode
 *
 * To cleanup the instance use:
 * @code
 * $ gcloud spanner instances delete ${INSTANCE_ID}
 *     --config=regional-us-central1 --description="${INSTANCE_ID}" --nodes=1
 * @endcode
 *
 * You can run the sub-commands of this tool to access this instance, for
 * example, to list the databases in your instance use:
 *
 * @code
 * $ bazel run google/cloud/spanner:spanner_tool --
 *     list-databases ${PROJECT_ID} ${INSTANCE_ID}
 * @endcode
 *
 * Naturally this list is initially empty, to create a database and list it:
 *
 * @code
 * $ bazel run google/cloud/spanner:spanner_tool --
 *     create-database ${PROJECT_ID} ${INSTANCE_ID} testdb
 * $ bazel run google/cloud/spanner:spanner_tool --
 *     list-databases ${PROJECT_ID} ${INSTANCE_ID}
 * @endcode
 *
 * Once you have created a database you can create a table and insert
 * (currently a single row) into it:
 *
 * @code
 * $ bazel run google/cloud/spanner:spanner_tool --
 *     create-timeseries-table ${PROJECT_ID} ${INSTANCE_ID} testdb
 * $ bazel run google/cloud/spanner:spanner_tool --
 *     populate-timeseries ${PROJECT_ID} ${INSTANCE_ID} testdb
 * @endcode
 *
 */
int main(int argc, char* argv[]) {
  using CommandType = std::function<int(std::vector<std::string> args)>;

  std::map<std::string, CommandType> commands = {
      {"list-databases", &ListDatabases},
      {"create-database", &CreateDatabase},
      {"drop-database", &DropDatabase},
      {"create-timeseries-table", &CreateTimeseriesTable},
      {"populate-timeseries", &PopulateTimeseriesTable},
  };

  if (argc < 2) {
    std::cerr << argv[0] << ": missing command\n"
              << "Usage: " << argv[0] << " <command-name> [command-arguments]\n"
              << "Valid commands are:\n";
    for (auto const& kv : commands) {
      // Calling the command with an empty list always prints their usage.
      kv.second({});
    }
    return 1;
  }

  std::vector<std::string> args;
  std::transform(argv + 2, argv + argc, std::back_inserter(args),
                 [](char* x) { return std::string(x); });

  std::string const command_name = argv[1];
  auto command = commands.find(command_name);
  if (commands.end() == command) {
    std::cerr << argv[0] << ": unknown command " << command_name << '\n';
    for (auto const& kv : commands) {
      // Calling the command with an empty list always prints their usage.
      kv.second({});
    }
    return 1;
  }

  // Run the requested command.
  return command->second(args);
}
