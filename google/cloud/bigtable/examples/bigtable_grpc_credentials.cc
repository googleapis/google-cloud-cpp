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

#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/internal/getenv.h"
#include <fstream>
#include <sstream>

namespace {
struct Usage {
  std::string msg;
};

std::string command_usage;

void PrintUsage(std::string const& cmd, std::string const& msg) {
  auto last_slash = cmd.find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << command_usage << "\n";
}

void AccessToken(std::vector<std::string> argv) {
  if (argv.size() != 3) {
    throw Usage{"test-access-token: <project-id> <instance-id> <access-token>"};
  }

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
  (argv[0], argv[1], argv[2]);
}

void JWTAccessToken(std::vector<std::string> argv) {
  if (argv.size() != 3) {
    throw Usage{
        "test-jwt-access-token <project-id> <instance-id> "
        "<service_account_file_json>"};
  }
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
  (argv[0], argv[1], argv[2]);
}

void GCECredentials(std::vector<std::string> argv) {
  if (argv.size() != 2) {
    throw Usage{"test-gce-credentials: <project-id> <instance-id>"};
  }
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
  (argv[0], argv[1]);
}

void RunAll(std::vector<std::string> argv) {
  if (!argv.empty()) throw Usage{"auto"};
  for (auto const& var :
       {"GOOGLE_CLOUD_PROJECT", "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID",
        "ACCESS_TOKEN", "GOOGLE_APPLICATION_CREDENTIALS"}) {
    auto const value = google::cloud::internal::GetEnv(var);
    if (!value) {
      throw std::runtime_error("The " + std::string(var) +
                               " environment variable is not set");
    }
    if (value->empty()) {
      throw std::runtime_error("The " + std::string(var) +
                               " environment variable has an empty value");
    }
  }

  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  auto const instance_id = google::cloud::internal::GetEnv(
                               "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID")
                               .value_or("");
  auto const access_token =
      google::cloud::internal::GetEnv("ACCESS_TOKEN").value_or("");
  auto const credentials_file =
      google::cloud::internal::GetEnv("GOOGLE_APPLICATION_CREDENTIALS")
          .value_or("");

  AccessToken({project_id, instance_id, access_token});
  JWTAccessToken({project_id, instance_id, credentials_file});
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  using CommandType = std::function<void(std::vector<std::string>)>;

  std::map<std::string, CommandType> commands = {
      {"test-access-token", AccessToken},
      {"test-jwt-access-token", JWTAccessToken},
      {"test-gce-credentials", GCECredentials},
      {"auto", RunAll},
  };

  {
    // Force each command to generate its Usage string, so we can provide a good
    // usage string for the whole program. We need to create an InstanceAdmin
    // object to do this, but that object is never used, it is passed to the
    // commands, without any calls made to it.
    for (auto&& kv : commands) {
      try {
        if (kv.first == "auto") continue;
        kv.second({});
      } catch (Usage const& u) {
        command_usage += "    ";
        command_usage += u.msg;
        command_usage += "\n";
      }
    }
  }

  if (argc < 2) {
    PrintUsage(argv[0], "Missing command");
    return 1;
  }
  std::string const command_name = argv[1];
  std::vector<std::string> av(argv + 2, argv + argc);

  auto command = commands.find(command_name);
  if (commands.end() == command) {
    PrintUsage(command_name, "Unknown command: " + command_name);
    return 1;
  }

  command->second(av);

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argv[0], ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}
