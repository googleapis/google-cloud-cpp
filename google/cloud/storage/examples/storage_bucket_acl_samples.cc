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
#include <map>

namespace {

void ListBucketAcl(google::cloud::storage::Client client,
                   std::vector<std::string> const& argv) {
  //! [list bucket acl] [START storage_print_bucket_acl]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<std::vector<gcs::BucketAccessControl>> items =
        client.ListBucketAcl(bucket_name);

    if (!items) throw std::runtime_error(items.status().message());
    std::cout << "ACLs for bucket=" << bucket_name << "\n";
    for (gcs::BucketAccessControl const& acl : *items) {
      std::cout << acl.role() << ":" << acl.entity() << "\n";
    }
  }
  //! [list bucket acl] [END storage_print_bucket_acl]
  (std::move(client), argv.at(0));
}

void CreateBucketAcl(google::cloud::storage::Client client,
                     std::vector<std::string> const& argv) {
  //! [create bucket acl]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& entity, std::string const& role) {
    StatusOr<gcs::BucketAccessControl> bucket_acl =
        client.CreateBucketAcl(bucket_name, entity, role);

    if (!bucket_acl) throw std::runtime_error(bucket_acl.status().message());
    std::cout << "Role " << bucket_acl->role() << " granted to "
              << bucket_acl->entity() << " on bucket " << bucket_acl->bucket()
              << "\n"
              << "Full attributes: " << *bucket_acl << "\n";
  }
  //! [create bucket acl]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void DeleteBucketAcl(google::cloud::storage::Client client,
                     std::vector<std::string> const& argv) {
  //! [delete bucket acl]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& entity) {
    google::cloud::Status status = client.DeleteBucketAcl(bucket_name, entity);

    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Deleted ACL entry for " << entity << " in bucket "
              << bucket_name << "\n";
  }
  //! [delete bucket acl]
  (std::move(client), argv.at(0), argv.at(1));
}

void GetBucketAcl(google::cloud::storage::Client client,
                  std::vector<std::string> const& argv) {
  //! [get bucket acl] [START storage_print_bucket_acl_for_user]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& entity) {
    StatusOr<gcs::BucketAccessControl> acl =
        client.GetBucketAcl(bucket_name, entity);

    if (!acl) throw std::runtime_error(acl.status().message());
    std::cout << "ACL entry for " << acl->entity() << " in bucket "
              << acl->bucket() << " is " << *acl << "\n";
  }
  //! [get bucket acl] [END storage_print_bucket_acl_for_user]
  (std::move(client), argv.at(0), argv.at(1));
}

void UpdateBucketAcl(google::cloud::storage::Client client,
                     std::vector<std::string> const& argv) {
  //! [update bucket acl]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& entity, std::string const& role) {
    gcs::BucketAccessControl desired_acl =
        gcs::BucketAccessControl().set_entity(entity).set_role(role);

    StatusOr<gcs::BucketAccessControl> updated_acl =
        client.UpdateBucketAcl(bucket_name, desired_acl);

    if (!updated_acl) throw std::runtime_error(updated_acl.status().message());
    std::cout << "Bucket ACL updated. The ACL entry for "
              << updated_acl->entity() << " in bucket " << updated_acl->bucket()
              << " is " << *updated_acl << "\n";
  }
  //! [update bucket acl]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void PatchBucketAcl(google::cloud::storage::Client client,
                    std::vector<std::string> const& argv) {
  //! [patch bucket acl]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& entity, std::string const& role) {
    StatusOr<gcs::BucketAccessControl> original_acl =
        client.GetBucketAcl(bucket_name, entity);

    if (!original_acl) {
      throw std::runtime_error(original_acl.status().message());
    }

    auto new_acl = *original_acl;
    new_acl.set_role(role);

    StatusOr<gcs::BucketAccessControl> patched_acl =
        client.PatchBucketAcl(bucket_name, entity, *original_acl, new_acl);

    if (!patched_acl) throw std::runtime_error(patched_acl.status().message());
    std::cout << "ACL entry for " << patched_acl->entity() << " in bucket "
              << patched_acl->bucket() << " is now " << *patched_acl << "\n";
  }
  //! [patch bucket acl]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void PatchBucketAclNoRead(google::cloud::storage::Client client,
                          std::vector<std::string> const& argv) {
  //! [patch bucket acl no-read]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& entity, std::string const& role) {
    StatusOr<gcs::BucketAccessControl> patched_acl = client.PatchBucketAcl(
        bucket_name, entity,
        gcs::BucketAccessControlPatchBuilder().set_role(role));

    if (!patched_acl) throw std::runtime_error(patched_acl.status().message());
    std::cout << "ACL entry for " << patched_acl->entity() << " in bucket "
              << patched_acl->bucket() << " is now " << *patched_acl << "\n";
  }
  //! [patch bucket acl no-read]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void AddBucketOwner(google::cloud::storage::Client client,
                    std::vector<std::string> const& argv) {
  //! [add bucket owner] [START storage_add_bucket_owner]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& entity) {
    StatusOr<gcs::BucketAccessControl> patched_acl =
        client.PatchBucketAcl(bucket_name, entity,
                              gcs::BucketAccessControlPatchBuilder().set_role(
                                  gcs::BucketAccessControl::ROLE_OWNER()));

    if (!patched_acl) throw std::runtime_error(patched_acl.status().message());
    std::cout << "ACL entry for " << patched_acl->entity() << " in bucket "
              << patched_acl->bucket() << " is now " << *patched_acl << "\n";
  }
  //! [add bucket owner] [END storage_add_bucket_owner]
  (std::move(client), argv.at(0), argv.at(1));
}

void RemoveBucketOwner(google::cloud::storage::Client client,
                       std::vector<std::string> const& argv) {
  //! [remove bucket owner] [START storage_remove_bucket_owner]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& entity) {
    StatusOr<gcs::BucketMetadata> original_metadata =
        client.GetBucketMetadata(bucket_name, gcs::Projection::Full());

    if (!original_metadata) {
      throw std::runtime_error(original_metadata.status().message());
    }

    std::vector<gcs::BucketAccessControl> original_acl =
        original_metadata->acl();
    auto it = std::find_if(original_acl.begin(), original_acl.end(),
                           [entity](const gcs::BucketAccessControl& entry) {
                             return entry.entity() == entity &&
                                    entry.role() ==
                                        gcs::BucketAccessControl::ROLE_OWNER();
                           });

    if (it == original_acl.end()) {
      std::cout << "Could not find entity " << entity
                << " with role OWNER in bucket " << bucket_name << "\n";
      return;
    }

    gcs::BucketAccessControl owner = *it;
    google::cloud::Status status =
        client.DeleteBucketAcl(bucket_name, owner.entity());

    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Deleted ACL entry for " << owner.entity() << " in bucket "
              << bucket_name << "\n";
  }
  //! [remove bucket owner] [END storage_remove_bucket_owner]
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
  auto const bucket_name =
      examples::MakeRandomBucketName(generator, "cloud-cpp-test-examples-");
  auto const entity = "user-" + service_account;
  auto client = gcs::Client::CreateDefaultClient().value();
  std::cout << "\nCreating bucket to run the example (" << bucket_name << ")"
            << std::endl;
  auto bucket_metadata = client
                             .CreateBucketForProject(bucket_name, project_id,
                                                     gcs::BucketMetadata{})
                             .value();

  auto const reader = gcs::BucketAccessControl::ROLE_READER();
  auto const owner = gcs::BucketAccessControl::ROLE_OWNER();

  std::cout << "\nRunning ListBucketAcl() example" << std::endl;
  ListBucketAcl(client, {bucket_name});

  std::cout << "\nRunning CreateBucketAcl() example" << std::endl;
  CreateBucketAcl(client, {bucket_name, entity, reader});

  std::cout << "\nRunning GetBucketAcl() example" << std::endl;
  GetBucketAcl(client, {bucket_name, entity});

  std::cout << "\nRunning UpdateBucketAcl() example" << std::endl;
  UpdateBucketAcl(client, {bucket_name, entity, owner});

  std::cout << "\nRunning PatchBucketAcl() example" << std::endl;
  PatchBucketAcl(client, {bucket_name, entity, reader});

  std::cout << "\nRunning PatchBucketAcl() example" << std::endl;
  PatchBucketAclNoRead(client, {bucket_name, entity, owner});

  std::cout << "\nRunning DeleteBucketAcl() example" << std::endl;
  DeleteBucketAcl(client, {bucket_name, entity});

  std::cout << "\nRunning AddBucketOwner() example" << std::endl;
  AddBucketOwner(client, {bucket_name, entity});

  std::cout << "\nRunning RemoveBucketOwner() example" << std::endl;
  RemoveBucketOwner(client, {bucket_name, entity});

  (void)client.DeleteBucket(bucket_name);
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
      make_entry("list-bucket-acl", {}, ListBucketAcl),
      make_entry("create-bucket-acl", {"<entity>", "<role>"}, CreateBucketAcl),
      make_entry("delete-bucket-acl", {"<entity>"}, DeleteBucketAcl),
      make_entry("get-bucket-acl", {"<entity>"}, GetBucketAcl),
      make_entry("update-bucket-acl", {"<entity>", "<role>"}, UpdateBucketAcl),
      make_entry("patch-bucket-acl", {"<entity>", "<role>"}, PatchBucketAcl),
      make_entry("patch-bucket-acl-no-read", {"<entity>", "<role>"},
                 PatchBucketAclNoRead),
      make_entry("add-bucket-owner", {"<entity>"}, AddBucketOwner),
      make_entry("remove-bucket-owner", {"<entity>"}, RemoveBucketOwner),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
