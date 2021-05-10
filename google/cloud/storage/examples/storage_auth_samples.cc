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
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/parallel_upload.h"
#include "google/cloud/storage/well_known_parameters.h"
#include "google/cloud/internal/getenv.h"
#include <iostream>
#include <map>
#include <string>
#include <thread>

namespace {

void PerformSomeOperations(google::cloud::storage::Client client,
                           std::string const& bucket_name,
                           std::string const& object_name) {
  namespace gcs = google::cloud::storage;
  auto constexpr kText = "The quick brown fox jumps over the lazy dog\n";

  auto object = client.InsertObject(bucket_name, object_name, kText).value();
  for (auto&& o : client.ListObjects(bucket_name)) {
    if (!o) throw std::runtime_error(o.status().message());
    if (o->name() == object_name) break;
  }
  auto status = client.DeleteObject(bucket_name, object_name,
                                    gcs::Generation(object.generation()));
  if (!status.ok()) throw std::runtime_error(status.message());
}

void DefaultClient(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  if ((argv.size() == 1 && argv[0] == "--help") || argv.size() != 2) {
    throw examples::Usage{
        "default-client"
        " <bucket-name> <object-name>"};
  }
  //! [default-client]
  namespace gcs = google::cloud::storage;
  [](std::string const& bucket_name, std::string const& object_name) {
    auto client = gcs::Client::CreateDefaultClient();
    if (!client) throw std::runtime_error(client.status().message());

    PerformSomeOperations(*client, bucket_name, object_name);
  }
  //! [default-client]
  (argv.at(0), argv.at(1));
}

void ServiceAccountKeyfileJson(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  if ((argv.size() == 1 && argv[0] == "--help") || argv.size() != 3) {
    throw examples::Usage{
        "service-account-keyfile-json"
        " <service-account-file> <bucket-name> <object-name>"};
  }
  //! [service-account-keyfile-json]
  namespace gcs = google::cloud::storage;
  [](std::string const& filename, std::string const& bucket_name,
     std::string const& object_name) {
    auto credentials =
        gcs::oauth2::CreateServiceAccountCredentialsFromFilePath(filename);
    if (!credentials) throw std::runtime_error(credentials.status().message());

    PerformSomeOperations(gcs::Client(gcs::ClientOptions(*credentials)),
                          bucket_name, object_name);
  }
  //! [service-account-keyfile-json]
  (argv.at(0), argv.at(1), argv.at(2));
}

void ServiceAccountContentsJson(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  if ((argv.size() == 1 && argv[0] == "--help") || argv.size() != 3) {
    throw examples::Usage{
        "service-account-contents-json"
        " <service-account-file> <bucket-name> <object-name>"};
  }
  //! [service-account-contents-json]
  namespace gcs = google::cloud::storage;
  [](std::string const& filename, std::string const& bucket_name,
     std::string const& object_name) {
    auto is = std::ifstream(filename);
    is.exceptions(std::ios::badbit);
    auto const contents =
        std::string{std::istreambuf_iterator<char>(is.rdbuf()), {}};
    auto credentials =
        gcs::oauth2::CreateServiceAccountCredentialsFromJsonContents(contents);
    if (!credentials) throw std::runtime_error(credentials.status().message());

    PerformSomeOperations(gcs::Client(gcs::ClientOptions(*credentials)),
                          bucket_name, object_name);
  }
  //! [service-account-contents-json]
  (argv.at(0), argv.at(1), argv.at(2));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  namespace gcs = ::google::cloud::storage;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
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
  auto const delete_after =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);

  std::cout << "\nRunning DefaultClient()" << std::endl;
  auto const object_name = examples::MakeRandomObjectName(generator, "object-");
  DefaultClient({bucket_name, object_name});

  auto const filename = google::cloud::internal::GetEnv(
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON");
  if (filename.has_value()) {
    std::cout << "\nRunning ServiceAccountContentsJson()" << std::endl;
    ServiceAccountContentsJson({*filename, bucket_name, object_name});

    std::cout << "\nRunning ServiceAccountKeyfileJson()" << std::endl;
    ServiceAccountKeyfileJson({*filename, bucket_name, object_name});
  }

  if (!examples::UsingEmulator()) std::this_thread::sleep_until(delete_after);
  (void)examples::RemoveBucketAndContents(client, bucket_name);
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  examples::Example example({
      {"default-client", DefaultClient},
      {"service-account-contents-json", ServiceAccountContentsJson},
      {"service-account-keyfile-json", ServiceAccountKeyfileJson},
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
