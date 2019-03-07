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

#include "google/cloud/storage/client.h"
#include <functional>
#include <iostream>
#include <map>
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

void PrintUsage(int argc, char* argv[], std::string const& msg) {
  std::string const cmd = argv[0];
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << command_usage << "\n";
}

void GetServiceAccount(google::cloud::storage::Client client, int& argc,
                       char* argv[]) {
  if (argc != 1) {
    throw Usage{"get-service-account"};
  }
  //! [get service account] [START storage_get_service_account]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client) {
    StatusOr<gcs::ServiceAccount> service_account_details =
        client.GetServiceAccount();

    if (!service_account_details) {
      throw std::runtime_error(service_account_details.status().message());
    }
    std::cout << "The service account details are " << *service_account_details
              << "\n";
  }
  //! [get service account] [END storage_get_service_account]
  (std::move(client));
}

void GetServiceAccountForProject(google::cloud::storage::Client client,
                                 int& argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-service-account-for-project <project-id>"};
  }
  auto project_id = ConsumeArg(argc, argv);
  //! [get service account for project]
  // [START storage_get_service_account_for_project]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string project_id) {
    StatusOr<gcs::ServiceAccount> service_account_details =
        client.GetServiceAccountForProject(project_id);

    if (!service_account_details) {
      throw std::runtime_error(service_account_details.status().message());
    }

    std::cout << "The service account details for project " << project_id
              << " are " << *service_account_details << "\n";
  }
  // [END storage_get_service_account_for_project]
  //! [get service account for project]
  (std::move(client), project_id);
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  // Create a client to communicate with Google Cloud Storage.
  //! [create client]
  google::cloud::StatusOr<google::cloud::storage::Client> client =
      google::cloud::storage::Client::CreateDefaultClient();
  if (!client) {
    std::cerr << "Failed to create Storage Client, status=" << client.status()
              << "\n";
    return 1;
  }
  //! [create client]

  using CommandType =
      std::function<void(google::cloud::storage::Client, int&, char* [])>;
  std::map<std::string, CommandType> commands = {
      {"get-service-account", &GetServiceAccount},
      {"get-service-account-for-project", &GetServiceAccountForProject},
  };
  for (auto&& kv : commands) {
    try {
      int fake_argc = 0;
      kv.second(*client, fake_argc, argv);
    } catch (Usage const& u) {
      command_usage += "    ";
      command_usage += u.msg;
      command_usage += "\n";
    } catch (...) {
      // ignore other exceptions.
    }
  }

  if (argc < 2) {
    PrintUsage(argc, argv, "Missing command");
    return 1;
  }

  std::string const command = ConsumeArg(argc, argv);
  auto it = commands.find(command);
  if (commands.end() == it) {
    PrintUsage(argc, argv, "Unknown command: " + command);
    return 1;
  }

  it->second(*client, argc, argv);

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argc, argv, ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}
