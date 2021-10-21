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

#include "google/cloud/storage/examples/storage_examples_common.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
// [START storage_quickstart]
#include "google/cloud/storage/client.h"
#include <iostream>
// [END storage_quickstart]

namespace {

using google::cloud::storage::examples::Usage;

// [START storage_quickstart]
void StorageQuickstart(std::string const& bucket_name) {
  // Create an aliases to make the code easier to read.
  namespace gcs = google::cloud::storage;

  // Create a client to communicate with Google Cloud Storage. This client
  // uses the default configuration for authentication and project id.
  auto client = gcs::Client();

  // Create a bucket
  google::cloud::StatusOr<gcs::BucketMetadata> metadata = client.CreateBucket(
      bucket_name, gcs::BucketMetadata().set_location("US").set_storage_class(
                       gcs::storage_class::Standard()));
  if (!metadata) throw std::runtime_error(metadata.status().message());

  std::cout << "Created bucket " << metadata->name() << "\n";
}
// [END storage_quickstart]

void StorageQuickstartCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 1 || argv[0] == "--help") {
    throw Usage{"storage-quickstart <bucket-name>"};
  }
  StorageQuickstart(argv[0]);
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

  std::cout << "\nRunning StorageQuickStart() example" << std::endl;
  StorageQuickstartCommand({bucket_name});

  auto client = gcs::Client();
  (void)examples::RemoveBucketAndContents(client, bucket_name);
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  examples::Example example({
      {"storage-quickstart", StorageQuickstartCommand},
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
