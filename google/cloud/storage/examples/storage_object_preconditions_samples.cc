// Copyright 2021 Google LLC
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
#include "google/cloud/storage/well_known_parameters.h"
#include "google/cloud/internal/getenv.h"
#include <iostream>
#include <map>
#include <string>
#include <thread>

namespace {

void InsertOnlyIfDoesNotExists(google::cloud::storage::Client client,
                               std::vector<std::string> const& argv) {
  //! [insert-only-if-does-not-exists]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
    auto metadata = client.InsertObject(
        bucket_name, object_name, "The quick brown fox jumps over the lazy dog",
        gcs::IfGenerationMatch(0));
    if (!metadata) throw std::runtime_error(metadata.status().message());

    std::cout << "The object " << metadata->name() << " was created in bucket "
              << metadata->bucket() << "\nFull metadata: " << *metadata << "\n";
  }
  //! [insert-only-if-does-not-exists]
  (std::move(client), argv.at(0), argv.at(1));
}

void ReadObjectIfGenerationMatch(google::cloud::storage::Client client,
                                 std::vector<std::string> const& argv) {
  //! [read-object-if-generation-match]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::int64_t generation) {
    auto is = client.ReadObject(bucket_name, object_name,
                                gcs::IfGenerationMatch(generation));
    std::string line;
    while (std::getline(is, line)) {
      std::cout << line << "\n";
    }
    if (!is.status().ok()) throw std::runtime_error(is.status().message());
  }
  //! [read-object-if-generation-match]
  (std::move(client), argv.at(0), argv.at(1), std::stoll(argv.at(2)));
}

void ReadObjectIfMetagenerationMatch(google::cloud::storage::Client client,
                                     std::vector<std::string> const& argv) {
  //! [read-object-if-metageneration-match]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::int64_t metageneration) {
    auto is = client.ReadObject(bucket_name, object_name,
                                gcs::IfMetagenerationMatch(metageneration));
    std::string line;
    while (std::getline(is, line)) {
      std::cout << line << "\n";
    }
    if (!is.status().ok()) throw std::runtime_error(is.status().message());
  }
  //! [read-object-if-metageneration-match]
  (std::move(client), argv.at(0), argv.at(1), std::stoll(argv.at(2)));
}

void ReadObjectIfGenerationNotMatch(google::cloud::storage::Client client,
                                    std::vector<std::string> const& argv) {
  //! [read-object-if-generation-not-match]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::int64_t generation) {
    auto is = client.ReadObject(bucket_name, object_name,
                                gcs::IfGenerationNotMatch(generation));
    std::string line;
    while (std::getline(is, line)) {
      std::cout << line << "\n";
    }
    if (!is.status().ok()) throw std::runtime_error(is.status().message());
  }
  //! [read-object-if-generation-not-match]
  (std::move(client), argv.at(0), argv.at(1), std::stoll(argv.at(2)));
}

void ReadObjectIfMetagenerationNotMatch(google::cloud::storage::Client client,
                                        std::vector<std::string> const& argv) {
  //! [read-object-if-metageneration-not-match]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::int64_t metageneration) {
    auto is = client.ReadObject(bucket_name, object_name,
                                gcs::IfMetagenerationNotMatch(metageneration));
    std::string line;
    while (std::getline(is, line)) {
      std::cout << line << "\n";
    }
    if (!is.status().ok()) throw std::runtime_error(is.status().message());
  }
  //! [read-object-if-metageneration-not-match]
  (std::move(client), argv.at(0), argv.at(1), std::stoll(argv.at(2)));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  namespace gcs = ::google::cloud::storage;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME",
  });
  auto const bucket_name = google::cloud::internal::GetEnv(
                               "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                               .value();
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto client = gcs::Client();

  std::string const object_media("The quick brown fox jumps over the lazy dog");
  auto const object_name = examples::MakeRandomObjectName(generator, "object-");

  std::cout << "\nRunning InsertOnlyIfDoesNotExists() example" << std::endl;
  InsertOnlyIfDoesNotExists(client, {bucket_name, object_name, object_media});

  std::cout << "\nRunning ReadObjectIfGenerationMatch() example" << std::endl;
  auto object = client.GetObjectMetadata(bucket_name, object_name).value();
  ReadObjectIfGenerationMatch(
      client, {bucket_name, object_name, std::to_string(object.generation())});

  std::cout << "\nRunning ReadObjectIfMetagenerationMatch() example"
            << std::endl;
  ReadObjectIfMetagenerationMatch(
      client,
      {bucket_name, object_name, std::to_string(object.metageneration())});

  std::cout << "\nRunning ReadObjectIfGenerationNotMatch() example"
            << std::endl;
  ReadObjectIfGenerationNotMatch(
      client,
      {bucket_name, object_name, std::to_string(object.generation() + 1)});

  std::cout << "\nRunning ReadObjectIfMetagenerationNotMatch() example"
            << std::endl;
  ReadObjectIfMetagenerationNotMatch(
      client,
      {bucket_name, object_name, std::to_string(object.metageneration() + 1)});

  (void)client.DeleteObject(bucket_name, object_name,
                            gcs::Generation(object.generation()));
}
}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  auto make_entry = [](std::string const& name,
                       std::vector<std::string> arg_names,
                       examples::ClientCommand const& cmd) {
    arg_names.insert(arg_names.begin(), "<bucket-name>");
    return examples::CreateCommandEntry(name, std::move(arg_names), cmd);
  };

  examples::Example example({
      make_entry("insert-only-if-does-not-exists", {"<object-name>"},
                 InsertOnlyIfDoesNotExists),
      make_entry("read-object-if-generation-match",
                 {"<object-name>", "<generation>"},
                 ReadObjectIfGenerationMatch),
      make_entry("read-object-if-generation-not-match",
                 {"<object-name>", "<generation>"},
                 ReadObjectIfGenerationNotMatch),
      make_entry("read-object-if-metageneration-match",
                 {"<object-name>", "<metageneration>"},
                 ReadObjectIfMetagenerationMatch),
      make_entry("read-object-if-metageneration-not-match",
                 {"<object-name>", "<metageneration>"},
                 ReadObjectIfMetagenerationNotMatch),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
