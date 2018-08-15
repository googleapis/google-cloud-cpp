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

void ListDefaultObjectAcl(google::cloud::storage::Client client, int& argc,
                          char* argv[]) {
  if (argc != 2) {
    throw Usage{"list-default-object-acl <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [list default object acl] [START storage_print_bucket_default_acl]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name) {
    std::cout << "ACLs for bucket=" << bucket_name << std::endl;
    for (gcs::ObjectAccessControl const& acl :
         client.ListDefaultObjectAcl(bucket_name)) {
      std::cout << acl.role() << ":" << acl.entity() << std::endl;
    }
  }
  //! [list default object acl] [END storage_print_bucket_default_acl]
  (std::move(client), bucket_name);
}

void CreateDefaultObjectAcl(google::cloud::storage::Client client, int& argc,
                            char* argv[]) {
  if (argc != 4) {
    throw Usage{"create-bucket-acl <bucket-name> <entity> <role>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  auto role = ConsumeArg(argc, argv);
  //! [create default object acl] [START storage_add_default_owner]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string entity,
     std::string role) {
    gcs::ObjectAccessControl result =
        client.CreateDefaultObjectAcl(bucket_name, entity, role);
    std::cout << "Role " << result.role() << " will be granted default to "
              << result.entity() << " on any new object created on bucket "
              << result.bucket() << "\n"
              << "Full attributes: " << result << std::endl;
  }
  //! [create default object acl] [END storage_add_default_owner]
  (std::move(client), bucket_name, entity, role);
}

void DeleteDefaultObjectAcl(google::cloud::storage::Client client, int& argc,
                            char* argv[]) {
  if (argc != 3) {
    throw Usage{"delete-default-object-acl <bucket-name> <entity>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  //! [delete default object acl] [START storage_remove_bucket_default_owner]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string entity) {
    client.DeleteDefaultObjectAcl(bucket_name, entity);
    std::cout << "Deleted ACL entry for " << entity << " in bucket "
              << bucket_name << std::endl;
  }
  //! [delete default object acl] [END storage_remove_bucket_default_owner]
  (std::move(client), bucket_name, entity);
}

void GetDefaultObjectAcl(google::cloud::storage::Client client, int& argc,
                         char* argv[]) {
  if (argc != 3) {
    throw Usage{"get-default-object-acl <bucket-name> <entity>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  //! [get default object acl]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string entity) {
    gcs::ObjectAccessControl acl =
        client.GetDefaultObjectAcl(bucket_name, entity);
    std::cout << "Default Object ACL entry for " << entity << " in bucket "
              << bucket_name << " is " << acl << std::endl;
  }
  //! [get default object acl]
  (std::move(client), bucket_name, entity);
}

void UpdateDefaultObjectAcl(google::cloud::storage::Client client, int& argc,
                            char* argv[]) {
  if (argc != 4) {
    throw Usage{"update-default-object-acl <bucket-name> <entity> <role>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  auto role = ConsumeArg(argc, argv);
  //! [update default object acl]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string entity,
     std::string role) {
    gcs::ObjectAccessControl current_acl =
        client.GetDefaultObjectAcl(bucket_name, entity);
    current_acl.set_role(role);
    gcs::ObjectAccessControl acl =
        client.UpdateDefaultObjectAcl(bucket_name, current_acl);
    std::cout << "Default Object ACL entry for " << entity << " in bucket "
              << bucket_name << " is now " << acl << std::endl;
  }
  //! [update default object acl]
  (std::move(client), bucket_name, entity, role);
}

void PatchDefaultObjectAcl(google::cloud::storage::Client client, int& argc,
                           char* argv[]) {
  if (argc != 4) {
    throw Usage{"patch-default-object-acl <bucket-name> <entity> <role>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  auto role = ConsumeArg(argc, argv);
  //! [patch default object acl]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string entity,
     std::string role) {
    gcs::ObjectAccessControl original_acl =
        client.GetDefaultObjectAcl(bucket_name, entity);
    auto new_acl = original_acl;
    new_acl.set_role(role);
    gcs::ObjectAccessControl updated_acl =
        client.PatchDefaultObjectAcl(bucket_name, entity, original_acl, new_acl,
                                     gcs::IfMatchEtag(original_acl.etag()));
    std::cout << "Default Object ACL entry for " << entity << " in bucket "
              << bucket_name << " is now " << updated_acl << std::endl;
  }
  //! [patch default object acl]
  (std::move(client), bucket_name, entity, role);
}

void PatchDefaultObjectAclNoRead(google::cloud::storage::Client client,
                                 int& argc, char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "patch-default-object-acl-no-read <bucket-name> <entity> <role>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  auto role = ConsumeArg(argc, argv);
  //! [patch default object acl no-read]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string entity,
     std::string role) {
    gcs::ObjectAccessControl acl = client.PatchDefaultObjectAcl(
        bucket_name, entity,
        gcs::ObjectAccessControlPatchBuilder().set_role(role));
    std::cout << "Default Object ACL entry for " << entity << " in bucket "
              << bucket_name << " is now " << acl << std::endl;
  }
  //! [patch default object acl no-read]
  (std::move(client), bucket_name, entity, role);
}
}  // anonymous namespace

int main(int argc, char* argv[]) try {
  // Create a client to communicate with Google Cloud Storage.
  google::cloud::storage::Client client;

  // Build the list of commands and the usage string from that list.
  using CommandType =
      std::function<void(google::cloud::storage::Client, int&, char* [])>;
  std::map<std::string, CommandType> commands = {
      {"list-default-object-acl", &ListDefaultObjectAcl},
      {"create-default-object-acl", &CreateDefaultObjectAcl},
      {"delete-default-object-acl", &DeleteDefaultObjectAcl},
      {"get-default-object-acl", &GetDefaultObjectAcl},
      {"update-default-object-acl", &UpdateDefaultObjectAcl},
      {"patch-default-object-acl", &PatchDefaultObjectAcl},
      {"patch-default-object-acl-no-read", &PatchDefaultObjectAclNoRead},
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
