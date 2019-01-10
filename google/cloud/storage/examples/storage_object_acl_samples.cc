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

namespace gcs = google::cloud::storage;

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

void ListObjectAcl(gcs::Client client, int& argc, char* argv[]) {
  if (argc < 3) {
    throw Usage{"list-object-acl <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  //! [list object acl]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name) {
    std::cout << "ACLs for object=" << object_name << " in bucket "
              << bucket_name << std::endl;
    for (gcs::ObjectAccessControl const& acl :
         client.ListObjectAcl(bucket_name, object_name)) {
      std::cout << acl.role() << ":" << acl.entity() << std::endl;
    }
  }
  //! [list object acl]
  (std::move(client), bucket_name, object_name);
}

void CreateObjectAcl(gcs::Client client, int& argc, char* argv[]) {
  if (argc < 5) {
    throw Usage{
        "create-object-acl <bucket-name> <object-name> <entity> <role>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  auto role = ConsumeArg(argc, argv);
  //! [create object acl] [START storage_create_file_acl]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string entity, std::string role) {
    gcs::ObjectAccessControl result =
        client.CreateObjectAcl(bucket_name, object_name, entity, role);
    std::cout << "Role " << result.role() << " granted to " << result.entity()
              << " on " << result.object() << "\n"
              << "Full attributes: " << result << std::endl;
  }
  //! [create object acl] [END storage_create_file_acl]
  (std::move(client), bucket_name, object_name, entity, role);
}

void DeleteObjectAcl(gcs::Client client, int& argc, char* argv[]) {
  if (argc < 4) {
    throw Usage{"delete-object-acl <bucket-name> <object-name> <entity>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  //! [delete object acl] [START storage_delete_file_acl]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string entity) {
    client.DeleteObjectAcl(bucket_name, object_name, entity);
    std::cout << "Deleted ACL entry for " << entity << " in object "
              << object_name << " in bucket " << bucket_name << std::endl;
  }
  //! [delete object acl] [END storage_delete_file_acl]
  (std::move(client), bucket_name, object_name, entity);
}

void GetObjectAcl(gcs::Client client, int& argc, char* argv[]) {
  if (argc < 4) {
    throw Usage{"get-object-acl <bucket-name> <object-name> <entity>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  //! [get object acl] [START storage_print_file_acl]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string entity) {
    gcs::ObjectAccessControl acl =
        client.GetObjectAcl(bucket_name, object_name, entity);
    std::cout << "ACL entry for " << entity << " in object " << object_name
              << " in bucket " << bucket_name << " is " << acl << std::endl;
  }
  //! [get object acl] [END storage_print_file_acl]
  (std::move(client), bucket_name, object_name, entity);
}

void UpdateObjectAcl(gcs::Client client, int& argc, char* argv[]) {
  if (argc < 5) {
    throw Usage{
        "update-object-acl <bucket-name> <object-name> <entity> <role>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  auto role = ConsumeArg(argc, argv);
  //! [update object acl] [START storage_update_file_acl]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string entity, std::string role) {
    gcs::ObjectAccessControl current_acl =
        client.GetObjectAcl(bucket_name, object_name, entity);
    current_acl.set_role(role);
    gcs::ObjectAccessControl acl =
        client.UpdateObjectAcl(bucket_name, object_name, current_acl);
    std::cout << "ACL entry for " << entity << " in object " << object_name
              << " in bucket " << bucket_name << " is now " << acl << std::endl;
  }
  //! [update object acl] [END storage_update_file_acl]
  (std::move(client), bucket_name, object_name, entity, role);
}

void PatchObjectAcl(gcs::Client client, int& argc, char* argv[]) {
  if (argc != 5) {
    throw Usage{"patch-object-acl <bucket-name> <object-name> <entity> <role>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  auto role = ConsumeArg(argc, argv);
  //! [patch object acl]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string entity, std::string role) {
    gcs::ObjectAccessControl original_acl =
        client.GetObjectAcl(bucket_name, object_name, entity);
    auto new_acl = original_acl;
    new_acl.set_role(role);
    gcs::ObjectAccessControl updated_acl = client.PatchObjectAcl(
        bucket_name, object_name, entity, original_acl, new_acl);
    std::cout << "ACL entry for " << entity << " in object " << object_name
              << " in bucket " << bucket_name << " is now " << updated_acl
              << std::endl;
  }
  //! [patch object acl]
  (std::move(client), bucket_name, object_name, entity, role);
}

void PatchObjectAclNoRead(gcs::Client client, int& argc, char* argv[]) {
  if (argc != 5) {
    throw Usage{
        "patch-object-acl-no-read <bucket-name> <object-name> <entity> <role>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  auto role = ConsumeArg(argc, argv);
  //! [patch object acl no-read]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string entity, std::string role) {
    gcs::ObjectAccessControl acl = client.PatchObjectAcl(
        bucket_name, object_name, entity,
        gcs::ObjectAccessControlPatchBuilder().set_role(role));
    std::cout << "ACL entry for " << entity << " in object " << object_name
              << " in bucket " << bucket_name << " is now " << acl << std::endl;
  }
  //! [patch object acl no-read]
  (std::move(client), bucket_name, object_name, entity, role);
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  // Create a client to communicate with Google Cloud Storage.
  gcs::Client client;

  // Build the list of commands and the usage string from that list.
  using CommandType = std::function<void(gcs::Client, int&, char*[])>;
  std::map<std::string, CommandType> commands = {
      {"list-object-acl", &ListObjectAcl},
      {"create-object-acl", &CreateObjectAcl},
      {"delete-object-acl", &DeleteObjectAcl},
      {"get-object-acl", &GetObjectAcl},
      {"update-object-acl", &UpdateObjectAcl},
      {"patch-object-acl", &PatchObjectAcl},
      {"patch-object-acl-no-read", &PatchObjectAclNoRead},
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
