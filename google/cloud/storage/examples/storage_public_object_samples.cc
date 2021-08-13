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

namespace {

using ::google::cloud::storage::examples::Usage;

void MakeObjectPublic(google::cloud::storage::Client client,
                      std::vector<std::string> const& argv) {
  //! [make object public] [START storage_make_public]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
    StatusOr<gcs::ObjectMetadata> updated = client.PatchObject(
        bucket_name, object_name, gcs::ObjectMetadataPatchBuilder(),
        gcs::PredefinedAcl::PublicRead());

    if (!updated) throw std::runtime_error(updated.status().message());
    std::cout << "Object updated. The full metadata after the update is: "
              << *updated << "\n";
  }
  //! [make object public] [END storage_make_public]
  (std::move(client), argv.at(0), argv.at(1));
}

void ReadObjectUnauthenticated(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw Usage{"read-object-unauthenticated <bucket-name> <object-name>"};
  }
  //! [download_public_file] [START storage_download_public_file]
  namespace gcs = ::google::cloud::storage;
  [](std::string const& bucket_name, std::string const& object_name) {
    // Create a client that does not authenticate with the server.
    auto client = gcs::Client{
        google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
            google::cloud::MakeInsecureCredentials())};

    // Read an object, the object must have been made public.
    gcs::ObjectReadStream stream = client.ReadObject(bucket_name, object_name);

    int count = 0;
    std::string line;
    while (std::getline(stream, line, '\n')) {
      ++count;
    }
    std::cout << "The object has " << count << " lines\n";
  }
  //! [download_public_file] [END storage_download_public_file]
  (argv.at(0), argv.at(1));
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
      examples::MakeRandomObjectName(generator, "public-object-") + ".txt";
  auto client = gcs::Client();

  auto constexpr kText = R"""(A bit of text to store in the test object.
The actual contents are not interesting.
)""";
  std::cout << "\nCreating object to run the example (" << object_name << ")"
            << std::endl;
  auto meta = client.InsertObject(bucket_name, object_name, kText).value();

  std::cout << "\nRunning the MakeObjectPublic() example" << std::endl;
  MakeObjectPublic(client, {bucket_name, object_name});

  std::cout << "\nRunning the ReadObjectUnathenticated() example" << std::endl;
  ReadObjectUnauthenticated({bucket_name, object_name});

  (void)client.DeleteObject(bucket_name, object_name);
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  examples::Example example({
      examples::CreateCommandEntry("set-cors-configuration",
                                   {"<bucket-name>", "<object-name>"},
                                   MakeObjectPublic),
      {"remove-cors-configuration", ReadObjectUnauthenticated},
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
