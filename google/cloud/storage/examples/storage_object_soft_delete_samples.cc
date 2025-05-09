// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/examples/storage_examples_common.h"
#include "google/cloud/internal/getenv.h"
#include <iostream>
#include <vector>

namespace {

using ::google::cloud::StatusOr;
using ::google::cloud::storage::ObjectMetadata;

std::vector<ObjectMetadata> ListSoftDeletedObjects(
    google::cloud::storage::Client client,
    std::vector<std::string> const& argv) {
  //! [storage_list_soft_deleted_objects]
  namespace gcs = ::google::cloud::storage;
  return [](gcs::Client client, std::string const& bucket_name) {
    std::cout << "Listing soft-deleted objects in the bucket: " << bucket_name
              << "\n";
    std::vector<gcs::ObjectMetadata> objects;
    for (auto const& object_metadata :
         client.ListObjects(bucket_name, gcs::SoftDeleted(true))) {
      if (!object_metadata) throw std::move(object_metadata).status();
      std::cout << "Soft-deleted object" << (objects.size() + 1) << ": "
                << object_metadata->name() << "\n";
      objects.push_back(*object_metadata);
    }
    return objects;
  }
  //! [storage_list_soft_deleted_objects]
  (std::move(client), argv.at(0));
}

std::vector<int64_t> ListSoftDeletedObjectVersions(
    google::cloud::storage::Client client,
    std::vector<std::string> const& argv) {
  //! [storage_list_soft_deleted_object_versions]
  namespace gcs = ::google::cloud::storage;
  return [](gcs::Client client, std::string const& bucket_name,
            std::string const& object_name) {
    std::vector<int64_t> versions;
    for (auto const& object_metadata :
         client.ListObjects(bucket_name, gcs::SoftDeleted(true))) {
      if (!object_metadata) throw std::move(object_metadata).status();
      if (object_metadata->name() == object_name) {
        std::cout << "Version" << (versions.size() + 1)
                  << " of the soft-deleted object " << object_name << ": "
                  << object_metadata->generation() << "\n";
        versions.push_back(object_metadata->generation());
      }
    }
    return versions;
  }
  //! [storage_list_soft_deleted_object_versions]
  (std::move(client), argv.at(0), argv.at(1));
}

StatusOr<ObjectMetadata> RestoreSoftDeletedObject(
    google::cloud::storage::Client client,
    std::vector<std::string> const& argv) {
  //! [storage_restore_object]
  namespace gcs = ::google::cloud::storage;
  return [](gcs::Client client, std::string const& bucket_name,
            std::string const& object_name, std::int64_t generation) {
    auto object_metadata = client.GetObjectMetadata(bucket_name, object_name);
    if (object_metadata.ok()) {
      std::cout << "Skipping restoration since object already exists: "
                << object_metadata->name() << "\n";
      return object_metadata;
    }
    auto restored_object_metadata =
        client.RestoreObject(bucket_name, object_name, generation);
    if (!restored_object_metadata.ok())
      throw std::move(restored_object_metadata).status();
    std::cout << "Object successfully restored: "
              << restored_object_metadata->name()
              << " (generation: " << generation << ")\n";
    return restored_object_metadata;
  }
  //! [storage_restore_object]
  (std::move(client), argv.at(0), argv.at(1), std::stoll(argv.at(2)));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  namespace gcs = ::google::cloud::storage;

  if (!argv.empty()) throw examples::Usage{"auto"};
  std::cout << "\nSetup" << std::endl;
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto client = gcs::Client();
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const bucket_name = examples::MakeRandomBucketName(generator);
  std::cout << "Creating a bucket having versioning and soft-delete enabled: "
            << bucket_name << std::endl;
  auto bucket = client.CreateBucket(
      bucket_name,
      gcs::BucketMetadata{}.set_soft_delete_policy(
          std::chrono::seconds(10 * std::chrono::hours(24))),
      gcs::OverrideDefaultProject(project_id));
  auto object_name = examples::MakeRandomObjectName(generator, "object-");
  std::cout << "Inserting an object: " << object_name << std::endl;
  auto inserted_object_metadata =
      client.InsertObject(bucket_name, object_name, "Test data for object");
  if (!inserted_object_metadata.ok()) {
    std::cout << "Object insertion failed with status: "
              << inserted_object_metadata.status() << "\n";
  }
  std::cout << "Deleting the object: " << object_name << std::endl;
  auto delete_object_status = client.DeleteObject(bucket_name, object_name);
  if (!delete_object_status.ok()) {
    std::cout << "Object deletion failed with status: " << delete_object_status
              << "\n";
  }

  std::cout << "\nRunning the ListSoftDeletedObjects() example" << std::endl;
  auto objects = ListSoftDeletedObjects(client, {bucket_name});
  if (objects.empty()) {
    throw std::runtime_error(
        "Unexpectedly no soft-deleted objects found in the bucket.");
  }

  std::cout << "\nRunning the ListSoftDeletedObjectVersions() example"
            << std::endl;
  auto versions =
      ListSoftDeletedObjectVersions(client, {bucket_name, object_name});
  if (versions.empty()) {
    throw std::runtime_error(
        "Unexpectedly no versions found of the soft-deleted object.");
  }

  std::cout << "\nRunning the RestoreSoftDeletedObject() example" << std::endl;
  RestoreSoftDeletedObject(
      client, {bucket_name, object_name, std::to_string(versions[0])});

  std::cout << "\nCleanup" << std::endl;
  std::cout << "Disabling soft-delete policy of the bucket.\n";
  auto updated_bucket_metadata = client.UpdateBucket(
      bucket_name, gcs::BucketMetadata().set_soft_delete_policy(
                       gcs::BucketSoftDeletePolicy{std::chrono::seconds(0)}));
  if (!updated_bucket_metadata.ok()) {
    std::cout << "Bucket update failed with status: "
              << updated_bucket_metadata.status() << "\n";
  }
  auto obj_delete_status = client.DeleteObject(bucket_name, object_name);
  if (!obj_delete_status.ok()) {
    std::cout << "Object deletion failed with status: "
              << obj_delete_status << "\n";
  } else {
    std::cout << "Object deleted successfully.\n";
  }
  auto bucket_delete_status = client.DeleteBucket(bucket_name);
  if (!bucket_delete_status.ok()) {
    std::cout << "Bucket deletion failed with status: " << bucket_delete_status
              << "\n";
  } else {
    std::cout << "Bucket deleted successfully.\n";
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  auto make_entry = [](std::string const& name,
                       std::vector<std::string> const& arg_names,
                       examples::ClientCommand const& cmd) {
    return examples::CreateCommandEntry(name, arg_names, cmd);
  };
  examples::Example example({
      make_entry("list-soft-deleted-objects", {"<bucket-name>"},
                 ListSoftDeletedObjects),
      make_entry("list-soft-deleted-object-versions",
                 {"<bucket-name>", "<object-name>"},
                 ListSoftDeletedObjectVersions),
      make_entry("restore-soft-deleted-object",
                 {"<bucket-name>", "<object-name>", "<generation>"},
                 RestoreSoftDeletedObject),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
