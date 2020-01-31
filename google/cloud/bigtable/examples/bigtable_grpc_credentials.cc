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

//! [all code]

#include "google/cloud/bigtable/table_admin.h"
#include <fstream>
#include <sstream>

namespace {
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

void PrintUsage(int, char* argv[], std::string const& msg) {
  std::string const cmd = argv[0];
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << command_usage << "\n";
}

void AccessToken(int argc, char* argv[]) {
  if (argc != 4) {
    throw Usage{"test-access-token: <project-id> <instance-id> <access-token>"};
  }
  std::string project_id = ConsumeArg(argc, argv);
  std::string instance_id = ConsumeArg(argc, argv);
  std::string access_token = ConsumeArg(argc, argv);

  // Create a namespace alias to make the code easier to read.
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;

  //! [test access token]
  [](std::string project_id, std::string instance_id,
     std::string access_token) {
    auto call_credentials = grpc::AccessTokenCredentials(access_token);
    auto channel_credentials =
        grpc::SslCredentials(grpc::SslCredentialsOptions());
    auto credentials = grpc::CompositeChannelCredentials(channel_credentials,
                                                         call_credentials);

    cbt::TableAdmin admin(cbt::CreateDefaultAdminClient(
                              project_id, cbt::ClientOptions(credentials)),
                          instance_id);

    auto tables = admin.ListTables(cbt::TableAdmin::NAME_ONLY);
    if (!tables) {
      throw std::runtime_error(tables.status().message());
    }
  }
  //! [test access token]
  (std::move(project_id), std::move(instance_id), std::move(access_token));
}

void JWTAccessToken(int argc, char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "test-jwt-access-token: <project-id> <instance-id> "
        "<service_account_file_json>"};
  }
  std::string project_id = ConsumeArg(argc, argv);
  std::string instance_id = ConsumeArg(argc, argv);
  std::string service_account_file_json = ConsumeArg(argc, argv);

  // Create a namespace alias to make the code easier to read.
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;

  //! [test jwt access token]
  [](std::string project_id, std::string instance_id,
     std::string service_account_file_json) {
    std::ifstream stream(service_account_file_json);
    if (!stream.is_open()) {
      std::ostringstream os;
      os << __func__ << "(" << service_account_file_json
         << "): cannot open upload file source";
      throw std::runtime_error(os.str());
    }
    std::string json_key(std::istreambuf_iterator<char>{stream}, {});

    auto call_credentials =
        grpc::ServiceAccountJWTAccessCredentials(json_key, 6000);
    auto channel_credentials =
        grpc::SslCredentials(grpc::SslCredentialsOptions());
    auto credentials = grpc::CompositeChannelCredentials(channel_credentials,
                                                         call_credentials);
    cbt::TableAdmin admin(cbt::CreateDefaultAdminClient(
                              project_id, cbt::ClientOptions(credentials)),
                          instance_id);

    auto tables = admin.ListTables(cbt::TableAdmin::NAME_ONLY);
    if (!tables) {
      throw std::runtime_error(tables.status().message());
    }
  }
  //! [test jwt access token]
  (std::move(project_id), std::move(instance_id),
   std::move(service_account_file_json));
}

void GCECredentials(int argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"test-gce-credentials: <project-id> <instance-id>"};
  }
  std::string project_id = ConsumeArg(argc, argv);
  std::string instance_id = ConsumeArg(argc, argv);

  // Create a namespace alias to make the code easier to read.
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;

  //! [test gce credentials]
  [](std::string project_id, std::string instance_id) {
    auto call_credentials = grpc::GoogleComputeEngineCredentials();
    auto channel_credentials =
        grpc::SslCredentials(grpc::SslCredentialsOptions());
    auto credentials = grpc::CompositeChannelCredentials(channel_credentials,
                                                         call_credentials);
    cbt::TableAdmin admin(cbt::CreateDefaultAdminClient(
                              project_id, cbt::ClientOptions(credentials)),
                          instance_id);

    auto tables = admin.ListTables(cbt::TableAdmin::NAME_ONLY);
    if (!tables) {
      throw std::runtime_error(tables.status().message());
    }
  }
  //! [test gce credentials]
  (std::move(project_id), std::move(instance_id));
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  using CommandType = std::function<void(int, char*[])>;

  std::map<std::string, CommandType> commands = {
      {"test-access-token", &AccessToken},
      {"test-jwt-access-token", &JWTAccessToken},
      {"test-gce-credentials", &GCECredentials},
  };

  {
    // Force each command to generate its Usage string, so we can provide a good
    // usage string for the whole program. We need to create an InstanceAdmin
    // object to do this, but that object is never used, it is passed to the
    // commands, without any calls made to it.
    for (auto&& kv : commands) {
      try {
        int fake_argc = 0;
        kv.second(fake_argc, argv);
      } catch (Usage const& u) {
        command_usage += "    ";
        command_usage += u.msg;
        command_usage += "\n";
      }
    }
  }

  if (argc < 4) {
    PrintUsage(argc, argv, "Missing command and/or project-id");
    return 1;
  }

  std::string const command_name = ConsumeArg(argc, argv);

  auto command = commands.find(command_name);
  if (commands.end() == command) {
    PrintUsage(argc, argv, "Unknown command: " + command_name);
    return 1;
  }

  command->second(argc, argv);

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argc, argv, ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}

//! [all code]
