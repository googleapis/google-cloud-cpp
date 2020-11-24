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

void WriteObjectWithKmsKey(google::cloud::storage::Client client,
                           std::vector<std::string> const& argv) {
  //! [write object with kms key] [START storage_upload_with_kms_key]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::string const& kms_key_name) {
    gcs::ObjectWriteStream stream = client.WriteObject(
        bucket_name, object_name, gcs::KmsKeyName(kms_key_name));

    // Line numbers start at 1.
    for (int lineno = 1; lineno <= 10; ++lineno) {
      stream << lineno << ": placeholder text for CMEK example.\n";
    }

    stream.Close();

    StatusOr<gcs::ObjectMetadata> metadata = std::move(stream).metadata();
    if (!metadata) throw std::runtime_error(metadata.status().message());

    std::cout << "Successfully wrote to object " << metadata->name()
              << " its size is: " << metadata->size()
              << "\nFull metadata: " << *metadata << "\n";
  }
  //! [write object with kms key] [END storage_upload_with_kms_key]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void ObjectCsekToCmek(google::cloud::storage::Client client,
                      std::vector<std::string> const& argv) {
  //! [object csek to cmek] [START storage_object_csek_to_cmek]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::string const& old_csek_key_base64,
     std::string const& new_cmek_key_name) {
    StatusOr<gcs::ObjectMetadata> metadata = client.RewriteObjectBlocking(
        bucket_name, object_name, bucket_name, object_name,
        gcs::SourceEncryptionKey::FromBase64Key(old_csek_key_base64),
        gcs::DestinationKmsKeyName(new_cmek_key_name));
    if (!metadata) throw std::runtime_error(metadata.status().message());

    std::cout << "Changed object " << metadata->name() << " in bucket "
              << metadata->bucket()
              << " from using CSEK to CMEK key.\nFull Metadata: " << *metadata
              << "\n";
  }
  //! [object csek to cmek] [END storage_object_csek_to_cmek]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void GetObjectKmsKey(google::cloud::storage::Client client,
                     std::vector<std::string> const& argv) {
  //! [get object kms key] [START storage_object_get_kms_key]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
    StatusOr<gcs::ObjectMetadata> metadata =
        client.GetObjectMetadata(bucket_name, object_name);
    if (!metadata) throw std::runtime_error(metadata.status().message());

    std::cout << "KMS key on object " << metadata->name() << " in bucket "
              << metadata->bucket() << ": " << metadata->kms_key_name() << "\n";
  }
  //! [get object kms key] [END storage_object_get_kms_key]
  (std::move(client), argv.at(0), argv.at(1));
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

  auto const cmek_object_name =
      examples::MakeRandomObjectName(generator, "cmek-object-") + ".txt";
  std::cout << "\nRunning the WriteObjectWithKmsKey() example" << std::endl;
  WriteObjectWithKmsKey(client, {bucket_name, cmek_object_name, cmek_key});

  std::cout << "\nRunning the GetObjectKmsKey() example" << std::endl;
  GetObjectKmsKey(client, {bucket_name, cmek_object_name});

  std::cout << "\nReading back the contents" << std::endl;
  {
    auto r = client.ReadObject(bucket_name, cmek_object_name);
    std::cout << "  contents="
              << std::string{std::istreambuf_iterator<char>{r}, {}} << "\n";
  }

  std::cout << "\nDeleting the object" << std::endl;
  (void)client.DeleteObject(bucket_name, cmek_object_name);

  std::cout << "\nCreating an object with a CSEK" << std::endl;
  auto const csek_object_name =
      examples::MakeRandomObjectName(generator, "csek-object-") + ".txt";
  auto const csek = gcs::CreateKeyFromGenerator(generator);
  auto constexpr kText = R"""(Some text to read and write)""";
  auto meta = client
                  .InsertObject(bucket_name, csek_object_name, kText,
                                gcs::EncryptionKey(csek))
                  .value();

  std::cout << "\nRunning the ObjectCsekToCmek() example";
  ObjectCsekToCmek(client, {bucket_name, csek_object_name, csek.key, cmek_key});

  std::cout << "\nReading back the contents" << std::endl;
  {
    auto r = client.ReadObject(bucket_name, cmek_object_name);
    std::cout << "  contents="
              << std::string{std::istreambuf_iterator<char>{r}, {}} << "\n";
  }
  (void)client.DeleteObject(bucket_name, csek_object_name);

  if (!examples::UsingEmulator()) std::this_thread::sleep_until(pause);
  (void)examples::RemoveBucketAndContents(client, bucket_name);
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  auto make_entry = [](std::string const& name,
                       std::vector<std::string> arg_names,
                       examples::ClientCommand const& cmd) {
    arg_names.insert(arg_names.begin(), {"<bucket-name>", "<object-name>"});
    return examples::CreateCommandEntry(name, std::move(arg_names), cmd);
  };
  examples::Example example({
      make_entry("write-object-with-kms-key", {"<kms-key-name>"},
                 WriteObjectWithKmsKey),
      make_entry(
          "object-csek-to-cmek",
          {"<old-csek-encryption-key>", "<new-cmek-encryption-key-name>"},
          ObjectCsekToCmek),
      make_entry("get-object-kms-key", {}, GetObjectKmsKey),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
