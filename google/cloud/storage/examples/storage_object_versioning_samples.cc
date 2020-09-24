// Copyright 2020 Google LLC
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

void GetObjectVersioning(google::cloud::storage::Client client,
                         std::vector<std::string> const& argv) {
  //! [START storage_view_versioning_status]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> metadata =
        client.GetBucketMetadata(bucket_name);
    if (!metadata) throw std::runtime_error(metadata.status().message());

    if (metadata->versioning().has_value()) {
      std::cout << "Object versioning for bucket " << bucket_name << " is "
                << (metadata->versioning()->enabled ? "enabled" : "disabled")
                << "\n";
    } else {
      std::cout << "Object versioning for bucket " << bucket_name
                << " is disabled.\n";
    }
  }
  //! [END storage_view_versioning_status]
  (std::move(client), argv.at(0));
}

void EnableObjectVersioning(google::cloud::storage::Client client,
                            std::vector<std::string> const& argv) {
  //! [enable versioning] [START storage_enable_versioning]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);
    if (!original) throw std::runtime_error(original.status().message());

    StatusOr<gcs::BucketMetadata> patched = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetVersioning(
            gcs::BucketVersioning{true}),
        gcs::IfMetagenerationMatch(original->metageneration()));
    if (!patched) throw std::runtime_error(patched.status().message());

    if (patched->versioning().has_value()) {
      std::cout << "Object versioning for bucket " << bucket_name << " is "
                << (patched->versioning()->enabled ? "enabled" : "disabled")
                << "\n";
    } else {
      std::cout << "Object versioning for bucket " << bucket_name
                << " is disabled.\n";
    }
  }
  //! [END storage_enable_versioning]
  (std::move(client), argv.at(0));
}

void DisableObjectVersioning(google::cloud::storage::Client client,
                             std::vector<std::string> const& argv) {
  //! [START storage_disable_versioning]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);
    if (!original) throw std::runtime_error(original.status().message());

    StatusOr<gcs::BucketMetadata> patched = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetVersioning(
            gcs::BucketVersioning{false}),
        gcs::IfMetagenerationMatch(original->metageneration()));
    if (!patched) throw std::runtime_error(patched.status().message());

    if (patched->versioning().has_value()) {
      std::cout << "Object versioning for bucket " << bucket_name << " is "
                << (patched->versioning()->enabled ? "enabled" : "disabled")
                << "\n";
    } else {
      std::cout << "Object versioning for bucket " << bucket_name
                << " is disabled.\n";
    }
  }
  //! [END storage_disable_versioning]
  (std::move(client), argv.at(0));
}

void CopyVersionedObject(google::cloud::storage::Client client,
                         std::vector<std::string> const& argv) {
  // [START storage_copy_file_archived_generation]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& source_bucket_name,
     std::string const& source_object_name,
     std::string const& destination_bucket_name,
     std::string const& destination_object_name,
     std::int64_t source_object_generation) {
    StatusOr<gcs::ObjectMetadata> copy =
        client.CopyObject(source_bucket_name, source_object_name,
                          destination_bucket_name, destination_object_name,
                          gcs::SourceGeneration{source_object_generation});
    if (!copy) throw std::runtime_error(copy.status().message());

    std::cout << "Successfully copied " << source_object_name << " generation "
              << source_object_generation << " in bucket " << source_bucket_name
              << " to bucket " << copy->bucket() << " with name "
              << copy->name()
              << ".\nThe full metadata after the copy is: " << *copy << "\n";
  }
  // [END storage_copy_file_archived_generation]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3),
   std::stoll(argv.at(4)));
}

void DeleteVersionedObject(google::cloud::storage::Client client,
                           std::vector<std::string> const& argv) {
  //! [delete versioned object]
  // [START storage_delete_file_archived_generation]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::int64_t object_generation) {
    google::cloud::Status status = client.DeleteObject(
        bucket_name, object_name, gcs::Generation{object_generation});
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "Deleted " << object_name << " generation "
              << object_generation << " in bucket " << bucket_name << "\n";
  }
  // [END storage_delete_file_archived_generation]
  //! [delete_file_archived_generation]
  (std::move(client), argv.at(0), argv.at(1), std::stoll(argv.at(2)));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  namespace gcs = ::google::cloud::storage;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_CMEK_KEY",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const cmek_key =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_CMEK_KEY")
          .value();
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const bucket_name = examples::MakeRandomBucketName(generator);
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

  std::cout << "\nRunning the GetObjectVersioning() example [1]" << std::endl;
  GetObjectVersioning(client, {bucket_name});

  std::cout << "\nRunning the EnableObjectVersioning() example" << std::endl;
  EnableObjectVersioning(client, {bucket_name});

  std::cout << "\nRunning the GetObjectVersioning() example [2]" << std::endl;
  GetObjectVersioning(client, {bucket_name});

  auto const text = R"""(Some text to insert into the test objects.)""";
  auto const src_object_name =
      examples::MakeRandomObjectName(generator, "object-") + ".txt";
  auto const dst_object_name =
      examples::MakeRandomObjectName(generator, "object-") + ".txt";

  auto meta_1 = client.InsertObject(bucket_name, src_object_name, text).value();
  auto meta_2 = client.InsertObject(bucket_name, src_object_name, text).value();

  std::cout << "\nRunning the CopyVersionedObject() example" << std::endl;
  CopyVersionedObject(
      client, {meta_1.bucket(), meta_1.name(), bucket_name, dst_object_name,
               std::to_string(meta_1.generation())});

  std::cout << "\nRunning the Delete VersionedObject() example [1]"
            << std::endl;
  DeleteVersionedObject(client, {meta_1.bucket(), meta_1.name(),
                                 std::to_string(meta_1.generation())});

  std::cout << "\nRunning the Delete VersionedObject() example [2]"
            << std::endl;
  DeleteVersionedObject(client, {meta_2.bucket(), meta_2.name(),
                                 std::to_string(meta_2.generation())});

  std::cout << "\nRunning the DisableObjectVersioning() example" << std::endl;
  DisableObjectVersioning(client, {bucket_name});

  std::cout << "\nRunning the GetObjectVersioning() example [2]" << std::endl;
  GetObjectVersioning(client, {bucket_name});

  (void)client.DeleteObject(bucket_name, dst_object_name);
  if (!examples::UsingTestbench()) std::this_thread::sleep_until(pause);
  (void)examples::RemoveBucketAndContents(client, bucket_name);
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  examples::Example example({
      examples::CreateCommandEntry("get-object-versioning", {"<bucket-name>"},
                                   GetObjectVersioning),
      examples::CreateCommandEntry("enable-object-versioning",
                                   {"<bucket-name>"}, EnableObjectVersioning),
      examples::CreateCommandEntry("disable-object-versioning",
                                   {"<bucket-name>"}, DisableObjectVersioning),
      examples::CreateCommandEntry(
          "copy-versioned-object",
          {"<source-bucket-name>", "<source-object-name>",
           "<destination-bucket-name>", "<destination-object-name> ",
           "<source-object-generation>"},
          CopyVersionedObject),
      examples::CreateCommandEntry(
          "delete-versioned-object",
          {"<bucket-name>", "<object-name>", "<object-generation>"},
          DeleteVersionedObject),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
