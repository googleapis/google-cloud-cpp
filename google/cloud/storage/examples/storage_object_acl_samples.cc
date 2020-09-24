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
#include "google/cloud/storage/examples/storage_examples_common.h"
#include "google/cloud/internal/getenv.h"
#include <functional>
#include <iostream>
#include <thread>

namespace {

void ListObjectAcl(google::cloud::storage::Client client,
                   std::vector<std::string> const& argv) {
  //! [list object acl] [START storage_print_file_acl]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
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
  (std::move(client), argv.at(0), argv.at(1));
}

void CreateObjectAcl(google::cloud::storage::Client client,
                     std::vector<std::string> const& argv) {
  //! [create object acl]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::string const& entity,
     std::string const& role) {
    StatusOr<gcs::ObjectAccessControl> object_acl =
        client.CreateObjectAcl(bucket_name, object_name, entity, role);

    if (!object_acl) throw std::runtime_error(object_acl.status().message());
    std::cout << "Role " << object_acl->role() << " granted to "
              << object_acl->entity() << " on " << object_acl->object()
              << "\nFull attributes: " << *object_acl << "\n";
  }
  //! [create object acl]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void DeleteObjectAcl(google::cloud::storage::Client client,
                     std::vector<std::string> const& argv) {
  //! [delete object acl]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::string const& entity) {
    google::cloud::Status status =
        client.DeleteObjectAcl(bucket_name, object_name, entity);

    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Deleted ACL entry for " << entity << " in object "
              << object_name << " in bucket " << bucket_name << "\n";
  }
  //! [delete object acl]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void GetObjectAcl(google::cloud::storage::Client client,
                  std::vector<std::string> const& argv) {
  //! [print file acl for user] [START storage_print_file_acl_for_user]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::string const& entity) {
    StatusOr<gcs::ObjectAccessControl> acl =
        client.GetObjectAcl(bucket_name, object_name, entity);

    if (!acl) throw std::runtime_error(acl.status().message());
    std::cout << "ACL entry for " << acl->entity() << " in object "
              << acl->object() << " in bucket " << acl->bucket() << " is "
              << *acl << "\n";
  }
  //! [print file acl for user] [END storage_print_file_acl_for_user]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void UpdateObjectAcl(google::cloud::storage::Client client,
                     std::vector<std::string> const& argv) {
  //! [update object acl]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::string const& entity,
     std::string const& role) {
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
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void PatchObjectAcl(google::cloud::storage::Client client,
                    std::vector<std::string> const& argv) {
  //! [patch object acl]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::string const& entity,
     std::string const& role) {
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
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void PatchObjectAclNoRead(google::cloud::storage::Client client,
                          std::vector<std::string> const& argv) {
  //! [patch object acl no-read]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::string const& entity,
     std::string const& role) {
    StatusOr<gcs::ObjectAccessControl> patched_acl = client.PatchObjectAcl(
        bucket_name, object_name, entity,
        gcs::ObjectAccessControlPatchBuilder().set_role(role));

    if (!patched_acl) throw std::runtime_error(patched_acl.status().message());
    std::cout << "ACL entry for " << patched_acl->entity() << " in object "
              << patched_acl->object() << " in bucket " << patched_acl->bucket()
              << " is now " << *patched_acl << "\n";
  }
  //! [patch object acl no-read]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void AddObjectOwner(google::cloud::storage::Client client,
                    std::vector<std::string> const& argv) {
  //! [add file owner] [START storage_add_file_owner]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::string const& entity) {
    StatusOr<gcs::ObjectAccessControl> patched_acl =
        client.CreateObjectAcl(bucket_name, object_name, entity,
                               gcs::ObjectAccessControl::ROLE_OWNER());

    if (!patched_acl) throw std::runtime_error(patched_acl.status().message());
    std::cout << "ACL entry for " << patched_acl->entity() << " in object "
              << patched_acl->object() << " in bucket " << patched_acl->bucket()
              << " is now " << *patched_acl << "\n";
  }
  //! [add file owner] [END storage_add_file_owner]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void RemoveObjectOwner(google::cloud::storage::Client client,
                       std::vector<std::string> const& argv) {
  //! [remove file owner] [START storage_remove_file_owner]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::string const& entity) {
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
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  namespace gcs = ::google::cloud::storage;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_SERVICE_ACCOUNT",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const service_account =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_STORAGE_TEST_SERVICE_ACCOUNT")
          .value();
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const bucket_name = examples::MakeRandomBucketName(generator);
  auto const entity = "user-" + service_account;
  auto client = gcs::Client::CreateDefaultClient().value();
  std::cout << "\nCreating bucket to run the example (" << bucket_name << ")"
            << std::endl;
  (void)client
      .CreateBucketForProject(bucket_name, project_id, gcs::BucketMetadata{})
      .value();
  // In GCS a single project cannot create or delete buckets more often than
  // once every two seconds. We will pause until that time before deleting the
  // bucket.
  auto pause = std::chrono::steady_clock::now() + std::chrono::seconds(2);

  auto const object_name = examples::MakeRandomObjectName(generator, "object-");
  auto object_metadata =
      client
          .InsertObject(bucket_name, object_name,
                        "some-string-to-serve-as-object-media")
          .value();

  auto const reader = gcs::BucketAccessControl::ROLE_READER();
  auto const owner = gcs::BucketAccessControl::ROLE_OWNER();

  std::cout << "\nRunning ListObjectAcl() example" << std::endl;
  ListObjectAcl(client, {bucket_name, object_name});

  std::cout << "\nRunning CreateObjectAcl() example" << std::endl;
  CreateObjectAcl(client, {bucket_name, object_name, entity, reader});

  std::cout << "\nRunning GetObjectAcl() example [1]" << std::endl;
  GetObjectAcl(client, {bucket_name, object_name, entity});

  std::cout << "\nRunning UpdateObjectAcl() example" << std::endl;
  UpdateObjectAcl(client, {bucket_name, object_name, entity, owner});

  std::cout << "\nRunning PatchObjectAcl() example" << std::endl;
  PatchObjectAcl(client, {bucket_name, object_name, entity, reader});

  std::cout << "\nRunning PatchObjectAclNoRead() example" << std::endl;
  PatchObjectAclNoRead(client, {bucket_name, object_name, entity, owner});

  std::cout << "\nRunning DeleteObjectAcl() example" << std::endl;
  DeleteObjectAcl(client, {bucket_name, object_name, entity});

  std::cout << "\nRunning AddObjectOwner() example" << std::endl;
  AddObjectOwner(client, {bucket_name, object_name, entity});

  std::cout << "\nRunning GetObjectAcl() example [2]" << std::endl;
  GetObjectAcl(client, {bucket_name, object_name, entity});

  std::cout << "\nRunning RemoveObjectOwner() example" << std::endl;
  RemoveObjectOwner(client, {bucket_name, object_name, entity});

  (void)client.DeleteObject(bucket_name, object_name);
  if (!examples::UsingTestbench()) std::this_thread::sleep_until(pause);
  (void)examples::RemoveBucketAndContents(client, bucket_name);
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  auto make_entry = [](std::string const& name,
                       std::vector<std::string> arg_names,
                       examples::ClientCommand const& cmd) {
    arg_names.insert(arg_names.begin(), "<bucket-name>");
    return examples::CreateCommandEntry(name, std::move(arg_names), cmd);
  };

  examples::Example example({
      make_entry("list-object-acl", {"<object-name>"}, ListObjectAcl),
      make_entry("create-object-acl", {"<object-name>", "<entity>", "<role>"},
                 CreateObjectAcl),
      make_entry("delete-object-acl", {"<object-name>", "<entity>"},
                 DeleteObjectAcl),
      make_entry("get-object-acl", {"<object-name>", "<entity>"}, GetObjectAcl),
      make_entry("update-object-acl", {"<object-name>", "<entity>", "<role>"},
                 UpdateObjectAcl),
      make_entry("patch-object-acl", {"<object-name>", "<entity>", "<role>"},
                 PatchObjectAcl),
      make_entry("patch-object-acl-no-read",
                 {"<object-name>", "<entity>", "<role>"}, PatchObjectAclNoRead),
      make_entry("add-object-owner", {"<object-name>", "<entity>"},
                 AddObjectOwner),
      make_entry("remove-object-owner", {"<object-name>", "<entity>"},
                 RemoveObjectOwner),
      {"auto", RunAll},

  });
  return example.Run(argc, argv);
}
