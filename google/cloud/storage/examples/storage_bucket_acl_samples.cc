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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<std::vector<gcs::BucketAccessControl>> items =
        client.ListBucketAcl(bucket_name);
    if (not items.ok()) {
      std::cerr << "Error getting ACL entries for bucket " << bucket_name
                << ", status=" << items.status() << std::endl;
      return;
    }
    std::cout << "ACLs for bucket=" << bucket_name << std::endl;
    for (gcs::BucketAccessControl const& acl : *items) {
      std::cout << acl.role() << ":" << acl.entity() << std::endl;
    }
  }
  //! [list bucket acl] [END storage_print_bucket_acl]
  (std::move(client), bucket_name);
}

void CreateBucketAcl(google::cloud::storage::Client client, int& argc,
                     char* argv[]) {
  if (argc != 4) {
    throw Usage{"create-bucket-acl <bucket-name> <entity> <role>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  auto role = ConsumeArg(argc, argv);
  //! [create bucket acl] [START storage_create_bucket_acl]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string entity,
     std::string role) {
    StatusOr<gcs::BucketAccessControl> bucket_acl =
        client.CreateBucketAcl(bucket_name, entity, role);
    if (not bucket_acl.ok()) {
      std::cerr << "Failure getting bucket ACL for entity " << entity
                << " in bucket " << bucket_name
                << ", status=" << bucket_acl.status() << std::endl;
      return;
    }
    std::cout << "Role " << bucket_acl->role() << " granted to "
              << bucket_acl->entity() << " on bucket " << bucket_acl->bucket()
              << "\n"
              << "Full attributes: " << *bucket_acl << std::endl;
  }
  //! [create bucket acl] [END storage_create_bucket_acl]
  (std::move(client), bucket_name, entity, role);
}

void DeleteBucketAcl(google::cloud::storage::Client client, int& argc,
                     char* argv[]) {
  if (argc != 3) {
    throw Usage{"delete-bucket-acl <bucket-name> <entity>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  //! [delete bucket acl] [START storage_delete_bucket_acl]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string entity) {
    StatusOr<void> status = client.DeleteBucketAcl(bucket_name, entity);
    if (not status.ok()) {
      std::cerr << "Failure deleting ACL for entity " << entity << " in bucket "
                << bucket_name << ", status=" << status.status() << std::endl;
      return;
    }
    std::cout << "Deleted ACL entry for " << entity << " in bucket "
              << bucket_name << std::endl;
  }
  //! [delete bucket acl] [END storage_delete_bucket_acl]
  (std::move(client), bucket_name, entity);
}

void GetBucketAcl(google::cloud::storage::Client client, int& argc,
                  char* argv[]) {
  if (argc != 3) {
    throw Usage{"get-bucket-acl <bucket-name> <entity>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  //! [get bucket acl] [START storage_print_bucket_acl_for_user]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string entity) {
    StatusOr<gcs::BucketAccessControl> acl =
        client.GetBucketAcl(bucket_name, entity);
    if (not acl.ok()) {
      std::cerr << "Failure getting ACL for entity " << entity << " in bucket "
                << bucket_name << ", status=" << acl.status() << std::endl;
      return;
    }
    std::cout << "ACL entry for " << acl->entity() << " in bucket "
              << acl->bucket() << " is " << *acl << std::endl;
  }
  //! [get bucket acl] [END storage_print_bucket_acl_for_user]
  (std::move(client), bucket_name, entity);
}

void UpdateBucketAcl(google::cloud::storage::Client client, int& argc,
                     char* argv[]) {
  if (argc != 4) {
    throw Usage{"update-bucket-acl <bucket-name> <entity> <role>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  auto role = ConsumeArg(argc, argv);
  //! [update bucket acl]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string entity,
     std::string role) {
    gcs::BucketAccessControl desired_acl =
        gcs::BucketAccessControl().set_entity(entity).set_role(role);
    StatusOr<gcs::BucketAccessControl> updated_acl =
        client.UpdateBucketAcl(bucket_name, desired_acl);
    if (not updated_acl.ok()) {
      std::cerr << "Error updating ACL for entity " << entity << " in bucket "
                << bucket_name << ", status=" << updated_acl.status()
                << std::endl;
      return;
    }
    std::cout << "Bucket ACL updated. The ACL entry for "
              << updated_acl->entity() << " in bucket " << updated_acl->bucket()
              << " is " << *updated_acl << std::endl;
  }
  //! [update bucket acl]
  (std::move(client), bucket_name, entity, role);
}

void PatchBucketAcl(google::cloud::storage::Client client, int& argc,
                    char* argv[]) {
  if (argc != 4) {
    throw Usage{"patch-bucket-acl <bucket-name> <entity> <role>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  auto role = ConsumeArg(argc, argv);
  //! [patch bucket acl]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string entity,
     std::string role) {
    StatusOr<gcs::BucketAccessControl> original_acl =
        client.GetBucketAcl(bucket_name, entity);
    if (not original_acl.ok()) {
      std::cerr << "Failure getting ACL for entity " << entity << " in bucket "
                << bucket_name << ", status=" << original_acl.status()
                << std::endl;
      return;
    }
    auto new_acl = *original_acl;
    new_acl.set_role(role);
    StatusOr<gcs::BucketAccessControl> patched_acl =
        client.PatchBucketAcl(bucket_name, entity, *original_acl, new_acl);
    if (not patched_acl.ok()) {
      std::cerr << "Failure patching ACL for entity " << entity << " in bucket "
                << bucket_name << ", status=" << patched_acl.status()
                << std::endl;
      return;
    }
    std::cout << "ACL entry for " << patched_acl->entity() << " in bucket "
              << patched_acl->bucket() << " is now " << *patched_acl
              << std::endl;
  }
  //! [patch bucket acl]
  (std::move(client), bucket_name, entity, role);
}

void PatchBucketAclNoRead(google::cloud::storage::Client client, int& argc,
                          char* argv[]) {
  if (argc != 4) {
    throw Usage{"patch-bucket-acl-no-read <bucket-name> <entity> <role>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto entity = ConsumeArg(argc, argv);
  auto role = ConsumeArg(argc, argv);
  //! [patch bucket acl no-read]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string entity,
     std::string role) {
    StatusOr<gcs::BucketAccessControl> patched_acl = client.PatchBucketAcl(
        bucket_name, entity,
        gcs::BucketAccessControlPatchBuilder().set_role(role));
    if (not patched_acl.ok()) {
      std::cerr << "Failure patching ACL for entity " << entity << " in bucket "
                << bucket_name << ", status=" << patched_acl.status()
                << std::endl;
      return;
    }
    std::cout << "ACL entry for " << patched_acl->entity() << " in bucket "
              << patched_acl->bucket() << " is now " << *patched_acl
              << std::endl;
  }
  //! [patch bucket acl no-read]
  (std::move(client), bucket_name, entity, role);
}
}  // anonymous namespace

int main(int argc, char* argv[]) try {
  // Create a client to communicate with Google Cloud Storage.
  google::cloud::storage::Client client;

  // Build the list of commands and the usage string from that list.
  using CommandType =
      std::function<void(google::cloud::storage::Client, int&, char*[])>;
  std::map<std::string, CommandType> commands = {
      {"list-bucket-acl", &ListBucketAcl},
      {"create-bucket-acl", &CreateBucketAcl},
      {"delete-bucket-acl", &DeleteBucketAcl},
      {"get-bucket-acl", &GetBucketAcl},
      {"update-bucket-acl", &UpdateBucketAcl},
      {"patch-bucket-acl", &PatchBucketAcl},
      {"patch-bucket-acl-no-read", &PatchBucketAclNoRead},
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
