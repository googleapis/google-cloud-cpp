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
#include <iostream>

namespace {

void SetObjectEventBasedHold(google::cloud::storage::Client client,
                             std::vector<std::string> const& argv) {
  //! [set event based hold] [START storage_set_event_based_hold]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
    StatusOr<gcs::ObjectMetadata> original =
        client.GetObjectMetadata(bucket_name, object_name);
    if (!original) throw std::runtime_error(original.status().message());

    StatusOr<gcs::ObjectMetadata> updated = client.PatchObject(
        bucket_name, object_name,
        gcs::ObjectMetadataPatchBuilder().SetEventBasedHold(true),
        gcs::IfMetagenerationMatch(original->metageneration()));
    if (!updated) throw std::runtime_error(updated.status().message());

    std::cout << "The event hold for object " << updated->name()
              << " in bucket " << updated->bucket() << " is "
              << (updated->event_based_hold() ? "enabled" : "disabled") << "\n";
  }
  //! [set event based hold] [END storage_set_event_based_hold]
  (std::move(client), argv.at(0), argv.at(1));
}

void ReleaseObjectEventBasedHold(google::cloud::storage::Client client,
                                 std::vector<std::string> const& argv) {
  //! [release event based hold] [START storage_release_event_based_hold]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
    StatusOr<gcs::ObjectMetadata> original =
        client.GetObjectMetadata(bucket_name, object_name);
    if (!original) throw std::runtime_error(original.status().message());

    StatusOr<gcs::ObjectMetadata> updated = client.PatchObject(
        bucket_name, object_name,
        gcs::ObjectMetadataPatchBuilder().SetEventBasedHold(false),
        gcs::IfMetagenerationMatch(original->metageneration()));
    if (!updated) throw std::runtime_error(updated.status().message());

    std::cout << "The event hold for object " << updated->name()
              << " in bucket " << updated->bucket() << " is "
              << (updated->event_based_hold() ? "enabled" : "disabled") << "\n";
  }
  //! [release event based hold] [END storage_release_event_based_hold]
  (std::move(client), argv.at(0), argv.at(1));
}

void SetObjectTemporaryHold(google::cloud::storage::Client client,
                            std::vector<std::string> const& argv) {
  //! [set temporary hold] [START storage_set_temporary_hold]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
    StatusOr<gcs::ObjectMetadata> original =
        client.GetObjectMetadata(bucket_name, object_name);
    if (!original) throw std::runtime_error(original.status().message());

    StatusOr<gcs::ObjectMetadata> updated = client.PatchObject(
        bucket_name, object_name,
        gcs::ObjectMetadataPatchBuilder().SetTemporaryHold(true),
        gcs::IfMetagenerationMatch(original->metageneration()));
    if (!updated) throw std::runtime_error(updated.status().message());

    std::cout << "The temporary hold for object " << updated->name()
              << " in bucket " << updated->bucket() << " is "
              << (updated->temporary_hold() ? "enabled" : "disabled") << "\n";
  }
  //! [set temporary hold] [END storage_set_temporary_hold]
  (std::move(client), argv.at(0), argv.at(1));
}

void ReleaseObjectTemporaryHold(google::cloud::storage::Client client,
                                std::vector<std::string> const& argv) {
  //! [release temporary hold] [START storage_release_temporary_hold]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
    StatusOr<gcs::ObjectMetadata> original =
        client.GetObjectMetadata(bucket_name, object_name);

    if (!original) throw std::runtime_error(original.status().message());
    StatusOr<gcs::ObjectMetadata> updated = client.PatchObject(
        bucket_name, object_name,
        gcs::ObjectMetadataPatchBuilder().SetTemporaryHold(false),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!updated) throw std::runtime_error(updated.status().message());
    std::cout << "The temporary hold for object " << updated->name()
              << " in bucket " << updated->bucket() << " is "
              << (updated->temporary_hold() ? "enabled" : "disabled") << "\n";
  }
  //! [release temporary hold] [END storage_release_temporary_hold]
  (std::move(client), argv.at(0), argv.at(1));
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
  auto client = gcs::Client::CreateDefaultClient().value();

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const object_name_1 =
      examples::MakeRandomObjectName(generator, "object-");
  auto const object_name_2 =
      examples::MakeRandomObjectName(generator, "object-");
  auto const text = R"""(Some text to populate the test objects)""";

  std::cout << "\nCreating the EventBasedHold object" << std::endl;
  (void)client
      .InsertObject(bucket_name, object_name_1, text, gcs::IfGenerationMatch(0))
      .value();

  std::cout << "\nRunning the SetObjectEventBasedHold() example" << std::endl;
  SetObjectEventBasedHold(client, {bucket_name, object_name_1});

  std::cout << "\nRunning the ReleaseObjectEventBasedHold() example"
            << std::endl;
  ReleaseObjectEventBasedHold(client, {bucket_name, object_name_1});

  std::cout << "\nDeleting the EventBasedHold object" << std::endl;
  (void)client.DeleteObject(bucket_name, object_name_1);

  std::cout << "\nCreating the TemporaryHold object" << std::endl;
  (void)client
      .InsertObject(bucket_name, object_name_2, text, gcs::IfGenerationMatch(0))
      .value();

  std::cout << "\nRunning the SetObjectTemporaryHold() example" << std::endl;
  SetObjectTemporaryHold(client, {bucket_name, object_name_2});

  std::cout << "\nRunning the ReleaseObjectTemporaryHold() example"
            << std::endl;
  ReleaseObjectTemporaryHold(client, {bucket_name, object_name_2});

  std::cout << "\nDeleting the TemporaryHold object" << std::endl;
  (void)client.DeleteObject(bucket_name, object_name_2);
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  auto make_entry = [](std::string const& name,
                       examples::ClientCommand const& cmd) {
    return examples::CreateCommandEntry(
        name, {"<bucket-name>", "<object-name>"}, cmd);
  };
  examples::Example example({
      make_entry("set-event-based-hold", SetObjectEventBasedHold),
      make_entry("release-event-based-hold", ReleaseObjectEventBasedHold),
      make_entry("set-temporary-hold", SetObjectTemporaryHold),
      make_entry("release-temporary-hold", ReleaseObjectTemporaryHold),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
