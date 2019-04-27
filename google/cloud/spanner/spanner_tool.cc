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

#include "google/cloud/storage/internal/format_time_point.h"
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
  if (args.size() != 4U) {
    std::cerr << args[0] << ": list-databases <project> <instance>\n";
    return 1;
  }
  auto const& project = args[2];
  auto const& instance = args[3];

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
    std::this_thread::sleep_for(std::chrono::seconds(10));
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
  if (args.size() != 5U) {
    std::cerr << args[0]
              << ": create-timeseries-table <project> <instance> "
                 "<database>\n";
    return 1;
  }
  auto const& project = args[2];
  auto const& instance = args[3];
  auto const& database = args[4];

  namespace spanner = google::spanner::admin::database::v1;

  std::shared_ptr<grpc::ChannelCredentials> cred =
      grpc::GoogleDefaultCredentials();
  std::shared_ptr<grpc::Channel> channel =
      grpc::CreateChannel("spanner.googleapis.com", cred);
  auto stub = spanner::DatabaseAdmin::NewStub(channel);

  spanner::CreateDatabaseRequest request;
  google::longrunning::Operation operation;
  request.set_parent("projects/" + project + "/instances/" + instance);
  request.set_create_statement("CREATE DATABASE " + database);

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

int CreateTimeseriesTable(std::vector<std::string> args) {
  if (args.size() != 5U) {
    std::cerr << args[0]
              << ": create-timeseries-table <project> <instance> "
                 "<database>\n";
    return 1;
  }
  auto const& project = args[2];
  auto const& instance = args[3];
  auto const& database = args[4];

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
  if (args.size() != 5U) {
    std::cerr << args[0] << ": populate-timeseries <project> <instance>"
              << " <database>\n";
    return 1;
  }
  auto const& project = args[2];
  auto const& instance = args[3];
  auto const& database = args[4];

  std::string database_name = "projects/" + project + "/instances/" + instance +
                              "/databases/" + database;

  namespace spanner = google::spanner::v1;

  std::shared_ptr<grpc::ChannelCredentials> cred =
      grpc::GoogleDefaultCredentials();
  std::shared_ptr<grpc::Channel> channel =
      grpc::CreateChannel("spanner.googleapis.com", cred);
  auto stub = spanner::Spanner::NewStub(std::move(channel));

  spanner::Session session = [&] {
    spanner::Session session;
    spanner::CreateSessionRequest request;
    request.set_database(database_name);

    grpc::ClientContext context;
    grpc::Status status = stub->CreateSession(&context, request, &session);
    if (!status.ok()) {
      std::cerr << "FAILED: [" << status.error_code() << "] - "
                << status.error_message() << "\n"
                << request.DebugString() << "\n";
      std::exit(1);
    }
    return session;
  }();

  std::cout << "Session: " << session.name() << "\n";

  spanner::Transaction read_write_transaction = [&] {
    spanner::Transaction transaction;
    spanner::BeginTransactionRequest request;
    request.set_session(session.name());
    *request.mutable_options()->mutable_read_write() = {};

    grpc::ClientContext context;
    grpc::Status status =
        stub->BeginTransaction(&context, request, &transaction);
    if (!status.ok()) {
      std::cerr << "FAILED: [" << status.error_code() << "] - "
                << status.error_message() << "\n";
      std::exit(1);
    }
    return transaction;
  }();

  std::cout << "Transaction: " << read_write_transaction.id() << "\n";

  // TODO(coryan) - use this when we update the protos to have .set_seqno()
  // std::int64_t seqno = 0;
  auto insert_one = [&](std::string series_name,
                        std::chrono::system_clock::time_point ts,
                        std::int64_t value) {
    spanner::ExecuteSqlRequest request;
    request.set_session(session.name());
    request.mutable_transaction()->set_id(read_write_transaction.id());
    // request.set_seqno(seqno++);
    request.set_sql("INSERT INTO timeseries (name, ts, value)"
                    " VALUES (@name, @time, @value)");
    auto& fields = *request.mutable_params()->mutable_fields();
    fields["name"] = [](std::string s) {
      google::protobuf::Value v;
      v.set_string_value(std::move(s));
      return v;
    }(std::move(series_name));
    fields["time"] = [](std::chrono::system_clock::time_point ts) {
      google::protobuf::Value v;
      v.set_string_value(google::cloud::storage::internal::FormatRfc3339(ts));
      return v;
    }(ts);
    fields["value"] = [](std::int64_t x) {
      google::protobuf::Value v;
      v.set_string_value(std::to_string(x));
      return v;
    }(value);
    auto& types = *request.mutable_param_types();
    types["time"] = [] {
      spanner::Type t;
      t.set_code(spanner::TIMESTAMP);
      return t;
    }();

    grpc::ClientContext context;
    spanner::ResultSet result;
    grpc::Status status = stub->ExecuteSql(&context, request, &result);
    if (!status.ok()) {
      std::cerr << "INSERT INTO FAILED: [" << status.error_code() << "] - "
                << status.error_message() << "\n";
      std::exit(1);
    }

    std::cout << "INSERT = " << result.DebugString() << "\n";
  };

#if 0
  // TODO(coryan) - use this when we update the protos to have .set_seqno()
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
#else
  insert_one("some-time-series", std::chrono::system_clock::now(), 42);
#endif  // 0

  grpc::ClientContext context;
  spanner::CommitRequest request;
  spanner::CommitResponse response;
  request.set_session(session.name());
  request.set_transaction_id(read_write_transaction.id());
  grpc::Status status = stub->Commit(&context, request, &response);
  if (!status.ok()) {
    std::cerr << "COMMIT FAILED: [" << status.error_code() << "] - "
              << status.error_message() << "\n";
    std::exit(1);
  }
  std::cout << "COMMIT = " << response.DebugString() << "\n";

  return 0;
}

}  // namespace

// This is a command-line tool to let folks easily experiment with Spanner
// using C++. This works with bazel using a command like:
//
// $ bazel run google/cloud/spanner:spanner_tool --
//       jgm-cloud-cxx jgm-spanner-instance
//
// Currently, the above command will just invoke the "ListDatabases" RPC, which
// makes it equivalent to the following command:
//
// $ gcloud spanner databases list
//       --project jgm-cloud-cxx --instance jgm-spanner-instance
//
// NOTE: The actual project and instance names will vary for other users; These
// are just examples.
int main(int argc, char* argv[]) {
  using CommandType = std::function<int(std::vector<std::string> args)>;

  std::map<std::string, CommandType> commands = {
      {"list-databases", &ListDatabases},
      {"create-database", &CreateDatabase},
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
  std::transform(argv, argv + argc, std::back_inserter(args),
                 [](char* x) { return std::string(x); });

  std::string const command_name = args[1];
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
