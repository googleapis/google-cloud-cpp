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
            << command_usage << std::endl;
}

void ListBucketAcl(google::cloud::storage::Client client, int& argc,
                   char* argv[]) {
  if (argc != 2) {
    throw Usage{"list-bucket-acl <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [list bucket acl] [START storage_print_bucket_acl]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name) {
    std::cout << "ACLs for bucket=" << bucket_name << std::endl;
    for (gcs::BucketAccessControl const& acl :
         client.ListBucketAcl(bucket_name)) {
      std::cout << acl.role() << ":" << acl.entity() << std::endl;
    }
  }
  //! [list bucket acl] [END storage_print_bucket_acl]
  (std::move(client), bucket_name);
}

void GetBucketAcl(google::cloud::storage::Client client, int& argc,
                  char* argv[]) {
  if (argc != 3) {
    throw Usage{"get-bucket-acl <bucket-name> <entity>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  //! [get bucket acl]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string entity) {
    gcs::BucketAccessControl acl = client.GetBucketAcl(bucket_name, entity);
    std::cout << "ACL entry for " << entity << " in bucket " << bucket_name
              << " is " << acl << std::endl;
  }
  //! [get bucket acl]
  (std::move(client), bucket_name, entity);
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  // Create a client to communicate with Google Cloud Storage.
  google::cloud::storage::Client client;

  // Build the list of commands and the usage string from that list.
  using CommandType =
      std::function<void(google::cloud::storage::Client, int&, char* [])>;
  std::map<std::string, CommandType> commands = {
      {"list-bucket-acl", &ListBucketAcl},
  };
  for (auto&& kv : commands) {
    try {
      int fake_argc = 1;
      kv.second(client, fake_argc, argv);
    } catch (Usage const& u) {
      command_usage += "    ";
      command_usage += u.msg;
      command_usage += "\n";
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

  // Call the command with that client.
  it->second(client, argc, argv);

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argc, argv, ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
