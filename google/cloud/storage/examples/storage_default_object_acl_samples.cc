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

void ListDefaultObjectAcl(google::cloud::storage::Client client,
                          std::vector<std::string> const& argv) {
  //! [list default object acl]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<std::vector<gcs::ObjectAccessControl>> items =
        client.ListDefaultObjectAcl(bucket_name);

    if (!items) throw std::runtime_error(items.status().message());
    std::cout << "ACLs for bucket=" << bucket_name << "\n";
    for (gcs::ObjectAccessControl const& acl : *items) {
      std::cout << acl.role() << ":" << acl.entity() << "\n";
    }
  }
  //! [list default object acl]
  (std::move(client), argv.at(0));
}

void CreateDefaultObjectAcl(google::cloud::storage::Client client,
                            std::vector<std::string> const& argv) {
  //! [create default object acl] [START storage_add_bucket_default_owner]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& entity, std::string const& role) {
    StatusOr<gcs::ObjectAccessControl> default_object_acl =
        client.CreateDefaultObjectAcl(bucket_name, entity, role);

    if (!default_object_acl) {
      throw std::runtime_error(default_object_acl.status().message());
    }

    std::cout << "Role " << default_object_acl->role()
              << " will be granted default to " << default_object_acl->entity()
              << " on any new object created on bucket "
              << default_object_acl->bucket() << "\n"
              << "Full attributes: " << *default_object_acl << "\n";
  }
  //! [create default object acl] [END storage_add_bucket_default_owner]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void GetDefaultObjectAcl(google::cloud::storage::Client client,
                         std::vector<std::string> const& argv) {
  //! [get default object acl]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& entity) {
    StatusOr<gcs::ObjectAccessControl> acl =
        client.GetDefaultObjectAcl(bucket_name, entity);

    if (!acl) throw std::runtime_error(acl.status().message());
    std::cout << "Default Object ACL entry for " << acl->entity()
              << " in bucket " << acl->bucket() << " is " << *acl << "\n";
  }
  //! [get default object acl]
  (std::move(client), argv.at(0), argv.at(1));
}

void UpdateDefaultObjectAcl(google::cloud::storage::Client client,
                            std::vector<std::string> const& argv) {
  //! [update default object acl]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& entity, std::string const& role) {
    StatusOr<gcs::ObjectAccessControl> original_acl =
        client.GetDefaultObjectAcl(bucket_name, entity);

    if (!original_acl) {
      throw std::runtime_error(original_acl.status().message());
    }

    original_acl->set_role(role);

    StatusOr<gcs::ObjectAccessControl> updated_acl =
        client.UpdateDefaultObjectAcl(bucket_name, *original_acl);

    if (!updated_acl) {
      throw std::runtime_error(updated_acl.status().message());
    }

    std::cout << "Default Object ACL entry for " << updated_acl->entity()
              << " in bucket " << updated_acl->bucket() << " is now "
              << *updated_acl << "\n";
  }
  //! [update default object acl]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void PatchDefaultObjectAcl(google::cloud::storage::Client client,
                           std::vector<std::string> const& argv) {
  //! [patch default object acl]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& entity, std::string const& role) {
    StatusOr<gcs::ObjectAccessControl> original_acl =
        client.GetDefaultObjectAcl(bucket_name, entity);

    if (!original_acl) {
      throw std::runtime_error(original_acl.status().message());
    }

    auto new_acl = *original_acl;
    new_acl.set_role(role);

    StatusOr<gcs::ObjectAccessControl> patched_acl =
        client.PatchDefaultObjectAcl(bucket_name, entity, *original_acl,
                                     new_acl);

    if (!patched_acl) throw std::runtime_error(patched_acl.status().message());
    std::cout << "Default Object ACL entry for " << patched_acl->entity()
              << " in bucket " << patched_acl->bucket() << " is now "
              << *patched_acl << "\n";
  }
  //! [patch default object acl]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void PatchDefaultObjectAclNoRead(google::cloud::storage::Client client,
                                 std::vector<std::string> const& argv) {
  //! [patch default object acl no-read]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& entity, std::string const& role) {
    StatusOr<gcs::ObjectAccessControl> patched_acl =
        client.PatchDefaultObjectAcl(
            bucket_name, entity,
            gcs::ObjectAccessControlPatchBuilder().set_role(role));

    if (!patched_acl) throw std::runtime_error(patched_acl.status().message());
    std::cout << "Default Object ACL entry for " << patched_acl->entity()
              << " in bucket " << patched_acl->bucket() << " is now "
              << *patched_acl << "\n";
  }
  //! [patch default object acl no-read]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void DeleteDefaultObjectAcl(google::cloud::storage::Client client,
                            std::vector<std::string> const& argv) {
  //! [delete default object acl] [START storage_remove_bucket_default_owner]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& entity) {
    google::cloud::Status status =
        client.DeleteDefaultObjectAcl(bucket_name, entity);

    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Deleted ACL entry for " << entity << " in bucket "
              << bucket_name << "\n";
  }
  //! [delete default object acl] [END storage_remove_bucket_default_owner]
  (std::move(client), argv.at(0), argv.at(1));
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

  auto const reader = gcs::BucketAccessControl::ROLE_READER();
  auto const owner = gcs::BucketAccessControl::ROLE_OWNER();

  std::cout << "\nRunning ListDefaultObjectAcl() example" << std::endl;
  ListDefaultObjectAcl(client, {bucket_name});

  std::cout << "\nRunning CreateDefaultObjectAcl() example" << std::endl;
  CreateDefaultObjectAcl(client, {bucket_name, entity, reader});

  std::cout << "\nRunning GetDefaultObjectAcl() example" << std::endl;
  GetDefaultObjectAcl(client, {bucket_name, entity});

  std::cout << "\nRunning UpdateDefaultObjectAcl() example" << std::endl;
  UpdateDefaultObjectAcl(client, {bucket_name, entity, owner});

  std::cout << "\nRunning PatchDefaultObjectAcl() example" << std::endl;
  PatchDefaultObjectAcl(client, {bucket_name, entity, reader});

  std::cout << "\nRunning PatchDefaultObjectAclNoRead() example" << std::endl;
  PatchDefaultObjectAclNoRead(client, {bucket_name, entity, owner});

  std::cout << "\nRunning DeleteDefaultObjectAcl() example" << std::endl;
  DeleteDefaultObjectAcl(client, {bucket_name, entity});

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
      make_entry("list-default-object-acl", {}, ListDefaultObjectAcl),
      make_entry("create-default-object-acl", {"<entity>", "<role>"},
                 CreateDefaultObjectAcl),
      make_entry("get-default-object-acl", {"<entity>"}, GetDefaultObjectAcl),
      make_entry("update-default-object-acl", {"<entity>", "<role>"},
                 UpdateDefaultObjectAcl),
      make_entry("patch-default-object-acl", {"<entity>", "<role>"},
                 PatchDefaultObjectAcl),
      make_entry("patch-default-object-acl-no-read", {"<entity>", "<role>"},
                 PatchDefaultObjectAclNoRead),
      make_entry("delete-default-object-acl", {"<entity>"},
                 DeleteDefaultObjectAcl),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
