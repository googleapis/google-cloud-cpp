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
#include "google/cloud/storage/parallel_upload.h"
#include "google/cloud/internal/getenv.h"
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace {

void UploadFile(google::cloud::storage::Client client,
                std::vector<std::string> const& argv) {
  //! [upload file] [START storage_upload_file]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string file_name, std::string bucket_name,
     std::string object_name) {
    // Note that the client library automatically computes a hash on the
    // client-side to verify data integrity during transmission.
    StatusOr<gcs::ObjectMetadata> metadata = client.UploadFile(
        file_name, bucket_name, object_name, gcs::IfGenerationMatch(0));
    if (!metadata) throw std::runtime_error(metadata.status().message());

    std::cout << "Uploaded " << file_name << " to object " << metadata->name()
              << " in bucket " << metadata->bucket()
              << "\nFull metadata: " << *metadata << "\n";
  }
  //! [upload file] [END storage_upload_file]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void UploadFileResumable(google::cloud::storage::Client client,
                         std::vector<std::string> const& argv) {
  //! [upload file resumable]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string file_name, std::string bucket_name,
     std::string object_name) {
    // Note that the client library automatically computes a hash on the
    // client-side to verify data integrity during transmission.
    StatusOr<gcs::ObjectMetadata> metadata = client.UploadFile(
        file_name, bucket_name, object_name, gcs::IfGenerationMatch(0),
        gcs::NewResumableUploadSession());
    if (!metadata) throw std::runtime_error(metadata.status().message());

    std::cout << "Uploaded " << file_name << " to object " << metadata->name()
              << " in bucket " << metadata->bucket()
              << "\nFull metadata: " << *metadata << "\n";
  }
  //! [upload file resumable]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void ParallelUploadFile(google::cloud::storage::Client client,
                        std::vector<std::string> const& argv) {
  //! [parallel upload file]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string file_name, std::string bucket_name,
     std::string object_name) {
    // Pick a unique random prefix for the temporary objects created by the
    // parallel upload.
    auto prefix = gcs::CreateRandomPrefixName("");

    auto metadata = gcs::ParallelUploadFile(client, file_name, bucket_name,
                                            object_name, prefix, false);
    if (!metadata) throw std::runtime_error(metadata.status().message());

    std::cout << "Uploaded " << file_name << " to object " << metadata->name()
              << " in bucket " << metadata->bucket()
              << "\nFull metadata: " << *metadata << "\n";
  }
  //! [parallel upload file]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void DownloadFile(google::cloud::storage::Client client,
                  std::vector<std::string> const& argv) {
  //! [download file]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string file_name) {
    google::cloud::Status status =
        client.DownloadToFile(bucket_name, object_name, file_name);
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "Downloaded " << object_name << " to " << file_name << "\n";
  }
  //! [download file]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

std::string MakeRandomFilename(
    google::cloud::internal::DefaultPRNG& generator) {
  auto constexpr kMaxBasenameLength = 28;
  std::string const prefix = "f-";
  return prefix +
         google::cloud::internal::Sample(
             generator, static_cast<int>(kMaxBasenameLength - prefix.size()),
             "abcdefghijklmnopqrstuvwxyz0123456789") +
         ".txt";
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
  auto const filename_1 = MakeRandomFilename(generator);
  auto const object_name = examples::MakeRandomObjectName(generator, "ob-");
  auto client = gcs::Client::CreateDefaultClient().value();

  std::string const text = R"""(
Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor
incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis
nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.
Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu
fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
culpa qui officia deserunt mollit anim id est laborum.
)""";

  std::cout << "\nCreating file for upload [1]" << std::endl;
  std::ofstream(filename_1).write(text.data(), text.size());

  std::cout << "\nRunning the UploadFile() example" << std::endl;
  UploadFile(client, {filename_1, bucket_name, object_name});

  std::cout << "\nRunning the DownloadFile() example" << std::endl;
  DownloadFile(client, {bucket_name, object_name, filename_1});

  std::cout << "\nDeleting uploaded object" << std::endl;
  (void)client.DeleteObject(bucket_name, object_name);

  std::cout << "\nCreating file for upload [2]" << std::endl;
  std::ofstream(filename_1).write(text.data(), text.size());

  std::cout << "\nRunning the ParallelUploadFile() example" << std::endl;
  ParallelUploadFile(client, {filename_1, bucket_name, object_name});

  std::cout << "\nDeleting uploaded object" << std::endl;
  (void)client.DeleteObject(bucket_name, object_name);

  std::cout << "\nCreating file for upload [3]" << std::endl;
  std::ofstream(filename_1).write(text.data(), text.size());

  std::cout << "\nRunning the UploadFileResumable() example" << std::endl;
  UploadFileResumable(client, {filename_1, bucket_name, object_name});

  std::cout << "\nDeleting uploaded object" << std::endl;
  (void)client.DeleteObject(bucket_name, object_name);

  std::cout << "\nRemoving local file" << std::endl;
  std::remove(filename_1.c_str());
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  google::cloud::storage::examples::Example example({
      examples::CreateCommandEntry(
          "upload-file", {"<filename>", "<bucket-name>", "<object-name>"},
          UploadFile),
      examples::CreateCommandEntry(
          "upload-file-resumable",
          {"<filename>", "<bucket-name>", "<object-name>"},
          UploadFileResumable),
      examples::CreateCommandEntry(
          "parallel-upload-file",
          {"<filename>", "<bucket-name>", "<object-name>"}, ParallelUploadFile),
      examples::CreateCommandEntry(
          "download-file", {"<bucket-name>", "<object-name>", "<filename>"},
          DownloadFile),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
