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

void PrintUsage(int, char* argv[], std::string const& msg) {
  std::string const cmd = argv[0];
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << command_usage << "\n";
}

void ListObjectAcl(gcs::Client client, int& argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"list-object-acl <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  //! [list object acl] [START storage_print_file_acl]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name) {
    StatusOr<std::vector<gcs::ObjectAccessControl>> items =
        client.ListObjectAcl(bucket_name, object_name);

    if (!items) throw std::runtime_error(items.status().message());
    std::cout << "ACLs for object=" << object_name << " in bucket "
              << bucket_name << "\n";
    for (gcs::ObjectAccessControl const& acl : *items) {
      std::cout << acl.role() << ":" << acl.entity() << "\n";
    }
  }
  //! [list object acl] [END storage_print_file_acl]
  (std::move(client), bucket_name, object_name);
}

void CreateObjectAcl(gcs::Client client, int& argc, char* argv[]) {
  if (argc != 5) {
    throw Usage{
        "create-object-acl <bucket-name> <object-name> <entity> <role>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  auto role = ConsumeArg(argc, argv);
  //! [create object acl]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string entity, std::string role) {
    StatusOr<gcs::ObjectAccessControl> object_acl =
        client.CreateObjectAcl(bucket_name, object_name, entity, role);

    if (!object_acl) throw std::runtime_error(object_acl.status().message());
    std::cout << "Role " << object_acl->role() << " granted to "
              << object_acl->entity() << " on " << object_acl->object()
              << "\nFull attributes: " << *object_acl << "\n";
  }
  //! [create object acl]
  (std::move(client), bucket_name, object_name, entity, role);
}

void DeleteObjectAcl(gcs::Client client, int& argc, char* argv[]) {
  if (argc != 4) {
    throw Usage{"delete-object-acl <bucket-name> <object-name> <entity>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  //! [delete object acl]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string entity) {
    google::cloud::Status status =
        client.DeleteObjectAcl(bucket_name, object_name, entity);

    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Deleted ACL entry for " << entity << " in object "
              << object_name << " in bucket " << bucket_name << "\n";
  }
  //! [delete object acl]
  (std::move(client), bucket_name, object_name, entity);
}

void GetObjectAcl(gcs::Client client, int& argc, char* argv[]) {
  if (argc != 4) {
    throw Usage{"get-object-acl <bucket-name> <object-name> <entity>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  //! [print file acl for user] [START storage_print_file_acl_for_user]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string entity) {
    StatusOr<gcs::ObjectAccessControl> acl =
        client.GetObjectAcl(bucket_name, object_name, entity);

    if (!acl) throw std::runtime_error(acl.status().message());
    std::cout << "ACL entry for " << acl->entity() << " in object "
              << acl->object() << " in bucket " << acl->bucket() << " is "
              << *acl << "\n";
  }
  //! [print file acl for user] [END storage_print_file_acl_for_user]
  (std::move(client), bucket_name, object_name, entity);
}

void UpdateObjectAcl(gcs::Client client, int& argc, char* argv[]) {
  if (argc != 5) {
    throw Usage{
        "update-object-acl <bucket-name> <object-name> <entity> <role>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  auto role = ConsumeArg(argc, argv);
  //! [update object acl]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string entity, std::string role) {
    StatusOr<gcs::ObjectAccessControl> current_acl =
        client.GetObjectAcl(bucket_name, object_name, entity);

    if (!current_acl) throw std::runtime_error(current_acl.status().message());
    current_acl->set_role(role);

    StatusOr<gcs::ObjectAccessControl> updated_acl =
        client.UpdateObjectAcl(bucket_name, object_name, *current_acl);

    if (!updated_acl) throw std::runtime_error(updated_acl.status().message());
    std::cout << "ACL entry for " << updated_acl->entity() << " in object "
              << updated_acl->object() << " in bucket " << updated_acl->bucket()
              << " is now " << *updated_acl << "\n";
  }
  //! [update object acl]
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
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string entity, std::string role) {
    StatusOr<gcs::ObjectAccessControl> original_acl =
        client.GetObjectAcl(bucket_name, object_name, entity);

    if (!original_acl) {
      throw std::runtime_error(original_acl.status().message());
    }

    gcs::ObjectAccessControl new_acl = *original_acl;
    new_acl.set_role(role);

    StatusOr<gcs::ObjectAccessControl> patched_acl = client.PatchObjectAcl(
        bucket_name, object_name, entity, *original_acl, new_acl);

    if (!patched_acl) throw std::runtime_error(patched_acl.status().message());
    std::cout << "ACL entry for " << patched_acl->entity() << " in object "
              << patched_acl->object() << " in bucket " << patched_acl->bucket()
              << " is now " << *patched_acl << "\n";
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
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string entity, std::string role) {
    StatusOr<gcs::ObjectAccessControl> patched_acl = client.PatchObjectAcl(
        bucket_name, object_name, entity,
        gcs::ObjectAccessControlPatchBuilder().set_role(role));

    if (!patched_acl) throw std::runtime_error(patched_acl.status().message());
    std::cout << "ACL entry for " << patched_acl->entity() << " in object "
              << patched_acl->object() << " in bucket " << patched_acl->bucket()
              << " is now " << *patched_acl << "\n";
  }
  //! [patch object acl no-read]
  (std::move(client), bucket_name, object_name, entity, role);
}

void AddObjectOwner(gcs::Client client, int& argc, char* argv[]) {
  if (argc != 4) {
    throw Usage{"add-object-owner <bucket-name> <object-name> <entity>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  //! [add file owner] [START storage_add_file_owner]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string entity) {
    StatusOr<gcs::ObjectAccessControl> patched_acl =
        client.CreateObjectAcl(bucket_name, object_name, entity,
                               gcs::ObjectAccessControl::ROLE_OWNER());

    if (!patched_acl) throw std::runtime_error(patched_acl.status().message());
    std::cout << "ACL entry for " << patched_acl->entity() << " in object "
              << patched_acl->object() << " in bucket " << patched_acl->bucket()
              << " is now " << *patched_acl << "\n";
  }
  //! [add file owner] [END storage_add_file_owner]
  (std::move(client), bucket_name, object_name, entity);
}

void RemoveObjectOwner(gcs::Client client, int& argc, char* argv[]) {
  if (argc != 4) {
    throw Usage{"remove-object-owner <bucket-name> <object-name> <entity>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  //! [remove file owner] [START storage_remove_file_owner]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string entity) {
    StatusOr<gcs::ObjectMetadata> original_metadata = client.GetObjectMetadata(
        bucket_name, object_name, gcs::Projection::Full());

    if (!original_metadata) {
      throw std::runtime_error(original_metadata.status().message());
    }

    std::vector<gcs::ObjectAccessControl> original_acl =
        original_metadata->acl();
    auto it = std::find_if(original_acl.begin(), original_acl.end(),
                           [entity](const gcs::ObjectAccessControl& entry) {
                             return entry.entity() == entity &&
                                    entry.role() ==
                                        gcs::ObjectAccessControl::ROLE_OWNER();
                           });

    if (it == original_acl.end()) {
      std::cout << "Could not find entity " << entity << " for file "
                << object_name << " with role OWNER in bucket " << bucket_name
                << "\n";
      return;
    }

    gcs::ObjectAccessControl owner = *it;
    google::cloud::Status status =
        client.DeleteObjectAcl(bucket_name, object_name, owner.entity());

    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Deleted ACL entry for " << owner.entity() << " for file "
              << object_name << " in bucket " << bucket_name << "\n";
  }
  //! [remove file owner] [END storage_remove_file_owner]
  (std::move(client), bucket_name, object_name, entity);
}
}  // anonymous namespace

int main(int argc, char* argv[]) try {
  // Create a client to communicate with Google Cloud Storage.
  google::cloud::StatusOr<google::cloud::storage::Client> client =
      google::cloud::storage::Client::CreateDefaultClient();
  if (!client) {
    std::cerr << "Failed to create Storage Client, status=" << client.status()
              << "\n";
    return 1;
  }

  // Build the list of commands and the usage string from that list.
  using CommandType = std::function<void(gcs::Client, int&, char*[])>;
  std::map<std::string, CommandType> commands = {
      {"list-object-acl", ListObjectAcl},
      {"create-object-acl", CreateObjectAcl},
      {"delete-object-acl", DeleteObjectAcl},
      {"get-object-acl", GetObjectAcl},
      {"update-object-acl", UpdateObjectAcl},
      {"patch-object-acl", PatchObjectAcl},
      {"patch-object-acl-no-read", PatchObjectAclNoRead},
      {"add-object-owner", AddObjectOwner},
      {"remove-object-owner", RemoveObjectOwner},
  };
  for (auto&& kv : commands) {
    try {
      int fake_argc = 1;
      kv.second(*client, fake_argc, argv);
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
  it->second(*client, argc, argv);

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argc, argv, ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}
