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
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/parallel_upload.h"
#include "google/cloud/storage/well_known_parameters.h"
#include "google/cloud/internal/getenv.h"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>

namespace {

std::string GenerateEncryptionKey() {
  //! [generate encryption key] [START storage_generate_encryption_key]
  // Create a pseudo-random number generator (PRNG), this is included for
  // demonstration purposes only. You should consult your security team about
  // best practices to initialize PRNG. In particular, you should verify that
  // the C++ library and operating system provide enough entropy to meet the
  // security policies in your organization.

  // Use the Mersenne-Twister Engine in this example:
  //   https://en.cppreference.com/w/cpp/numeric/random/mersenne_twister_engine
  // Any C++ PRNG can be used below, the choice is arbitrary.
  using GeneratorType = std::mt19937_64;

  // Create the default random device to fetch entropy.
  std::random_device rd;

  // Compute how much entropy we need to initialize the GeneratorType:
  constexpr auto kRequiredEntropyWords =
      GeneratorType::state_size *
      (GeneratorType::word_size / std::numeric_limits<unsigned int>::digits);

  // Capture the entropy bits into a vector.
  std::vector<unsigned long> entropy(kRequiredEntropyWords);
  std::generate(entropy.begin(), entropy.end(), [&rd] { return rd(); });

  // Create the PRNG with the fetched entropy.
  std::seed_seq seed(entropy.begin(), entropy.end());

  // initialized with enough entropy such that the encryption keys are not
  // predictable. Note that the default constructor for all the generators in
  // the C++ standard library produce predictable keys.
  std::mt19937_64 gen(seed);

  namespace gcs = google::cloud::storage;
  gcs::EncryptionKeyData data = gcs::CreateKeyFromGenerator(gen);

  std::cout << "Base64 encoded key = " << data.key << "\n"
            << "Base64 encoded SHA256 of key = " << data.sha256 << "\n";
  //! [generate encryption key] [END storage_generate_encryption_key]
  return data.key;
}

void GenerateEncryptionKeyCommand(std::vector<std::string> const& argv) {
  if (!argv.empty() || argv.front() == "--help") {
    throw google::cloud::storage::examples::Usage{"generate-encryption-key"};
  }
  GenerateEncryptionKey();
}

void WriteEncryptedObject(google::cloud::storage::Client client,
                          std::vector<std::string> const& argv) {
  //! [insert encrypted object] [START storage_upload_encrypted_file]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string base64_aes256_key) {
    StatusOr<gcs::ObjectMetadata> object_metadata = client.InsertObject(
        bucket_name, object_name, "top secret",
        gcs::EncryptionKey::FromBase64Key(base64_aes256_key));

    if (!object_metadata) {
      throw std::runtime_error(object_metadata.status().message());
    }

    std::cout << "The object " << object_metadata->name()
              << " was created in bucket " << object_metadata->bucket()
              << "\nFull metadata: " << *object_metadata << "\n";
  }
  //! [insert encrypted object] [END storage_upload_encrypted_file]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void ReadEncryptedObject(google::cloud::storage::Client client,
                         std::vector<std::string> const& argv) {
  //! [read encrypted object] [START storage_download_encrypted_file]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string base64_aes256_key) {
    gcs::ObjectReadStream stream =
        client.ReadObject(bucket_name, object_name,
                          gcs::EncryptionKey::FromBase64Key(base64_aes256_key));

    std::string data(std::istreambuf_iterator<char>{stream}, {});
    std::cout << "The object contents are: " << data << "\n";
  }
  //! [read encrypted object] [END storage_download_encrypted_file]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void ComposeObjectFromEncryptedObjects(google::cloud::storage::Client client,
                                       std::vector<std::string> const& argv) {
  auto it = argv.cbegin();
  auto bucket_name = *it++;
  auto destination_object_name = *it++;
  auto base64_aes256_key = *it++;
  std::vector<google::cloud::storage::ComposeSourceObject> compose_objects;
  do {
    compose_objects.push_back({*it++, {}, {}});
  } while (it != argv.cend());

  //! [compose object csek]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name,
     std::string destination_object_name, std::string base64_aes256_key,
     std::vector<gcs::ComposeSourceObject> compose_objects) {
    StatusOr<gcs::ObjectMetadata> composed_object = client.ComposeObject(
        bucket_name, compose_objects, destination_object_name,
        gcs::EncryptionKey::FromBase64Key(base64_aes256_key));

    if (!composed_object) {
      throw std::runtime_error(composed_object.status().message());
    }

    std::cout << "Composed new object " << composed_object->name()
              << " in bucket " << composed_object->bucket()
              << "\nFull metadata: " << *composed_object << "\n";
  }
  //! [compose object csek]
  (std::move(client), bucket_name, destination_object_name, base64_aes256_key,
   std::move(compose_objects));
}

void CopyEncryptedObject(google::cloud::storage::Client client,
                         std::vector<std::string> const& argv) {
  //! [copy encrypted object]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string source_bucket_name,
     std::string source_object_name, std::string destination_bucket_name,
     std::string destination_object_name, std::string key_base64) {
    StatusOr<gcs::ObjectMetadata> new_copy_meta = client.CopyObject(
        source_bucket_name, source_object_name, destination_bucket_name,
        destination_object_name, gcs::EncryptionKey::FromBase64Key(key_base64));

    if (!new_copy_meta) {
      throw std::runtime_error(new_copy_meta.status().message());
    }

    std::cout << "Successfully copied " << source_object_name << " in bucket "
              << source_bucket_name << " to bucket " << new_copy_meta->bucket()
              << " with name " << new_copy_meta->name()
              << ".\nThe full metadata after the copy is: " << *new_copy_meta
              << "\n";
  }
  //! [copy encrypted object]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3),
   argv.at(4));
}

void RotateEncryptionKey(google::cloud::storage::Client client,
                         std::vector<std::string> const& argv) {
  //! [rotate encryption key] [START storage_rotate_encryption_key]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string old_key_base64, std::string new_key_base64) {
    StatusOr<gcs::ObjectMetadata> object_metadata =
        client.RewriteObjectBlocking(
            bucket_name, object_name, bucket_name, object_name,
            gcs::SourceEncryptionKey::FromBase64Key(old_key_base64),
            gcs::EncryptionKey::FromBase64Key(new_key_base64));

    if (!object_metadata) {
      throw std::runtime_error(object_metadata.status().message());
    }

    std::cout << "Rotated key on object " << object_metadata->name()
              << " in bucket " << object_metadata->bucket()
              << "\nFull Metadata: " << *object_metadata << "\n";
  }
  //! [rotate encryption key] [END storage_rotate_encryption_key]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  namespace gcs = ::google::cloud::storage;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const bucket_name = google::cloud::internal::GetEnv(
                               "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                               .value();
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const object_name =
      examples::MakeRandomObjectName(generator, "ob-resumable-upload-");

  auto client = gcs::Client::CreateDefaultClient().value();

  auto const encrypted_object_name =
      examples::MakeRandomObjectName(generator, "enc-obj-");
  auto const encrypted_composed_object_name =
      examples::MakeRandomObjectName(generator, "composed-enc-obj-");
  auto const encrypted_copied_object_name =
      examples::MakeRandomObjectName(generator, "copied-enc-obj-");

  std::cout << "\nRunning GenerateEncryptionKey() example" << std::endl;
  auto const key = GenerateEncryptionKey();

  std::cout << "\nRunning WriteEncryptedObject() example" << std::endl;
  WriteEncryptedObject(client, {bucket_name, encrypted_object_name, key});

  std::cout << "\nRunning ReadEncryptedObject() example [1]" << std::endl;
  ReadEncryptedObject(client, {bucket_name, encrypted_object_name, key});

  std::cout << "\nRunning ComposeObjectFromEncryptedObjects() example"
            << std::endl;
  ComposeObjectFromEncryptedObjects(
      client, {bucket_name, encrypted_composed_object_name, key,
               encrypted_object_name, encrypted_object_name});

  std::cout << "\nRunning ReadEncryptedObject() example [2]" << std::endl;
  ReadEncryptedObject(client,
                      {bucket_name, encrypted_composed_object_name, key});

  std::cout << "\nRunning CopyEncryptedObject() example" << std::endl;
  CopyEncryptedObject(client, {bucket_name, encrypted_object_name, bucket_name,
                               encrypted_copied_object_name, key});

  std::cout << "\nRunning ReadEncryptedObject() example [3]" << std::endl;
  ReadEncryptedObject(client, {bucket_name, encrypted_copied_object_name, key});

  std::cout << "\nRunning RotateEncryptionKey() example" << std::endl;
  auto const newkey = GenerateEncryptionKey();
  RotateEncryptionKey(client,
                      {bucket_name, encrypted_object_name, key, newkey});

  std::cout << "\nCleanup" << std::endl;
  (void)client.DeleteObject(bucket_name, encrypted_copied_object_name);
  (void)client.DeleteObject(bucket_name, encrypted_composed_object_name);
  (void)client.DeleteObject(bucket_name, encrypted_object_name);
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

  google::cloud::storage::examples::Example example({
      {"generate-encryption-key", GenerateEncryptionKeyCommand},
      make_entry("write-encrypted-object", {"<base64-encoded-aes256-key>"},
                 WriteEncryptedObject),
      make_entry("read-encrypted-object", {"<base64-encoded-aes256-key>"},
                 ReadEncryptedObject),
      make_entry("compose-object-from-encrypted-objects",
                 {"<base64-encoded-aes256-key>", "<source-object>",
                  "[source-object...]"},
                 ComposeObjectFromEncryptedObjects),
      examples::CreateCommandEntry(
          "copy-encrypted-object",
          {"<source-bucket-name>", "<source-object-name>",
           "<destination-bucket-name>", "<destination-object-name>",
           "<encryption-key-base64>"},
          CopyEncryptedObject),
      make_entry("rotate-encryption-key",
                 {"<old-encryption-key>", "<new-encryption-key>"},
                 RotateEncryptionKey),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}