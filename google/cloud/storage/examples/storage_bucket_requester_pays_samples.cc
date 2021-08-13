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

void GetBilling(google::cloud::storage::Client client,
                std::vector<std::string> const& argv) {
  //! [get billing] [START storage_get_requester_pays_status]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& user_project) {
    StatusOr<gcs::BucketMetadata> metadata =
        client.GetBucketMetadata(bucket_name, gcs::UserProject(user_project));
    if (!metadata) throw std::runtime_error(metadata.status().message());

    if (!metadata->has_billing()) {
      std::cout
          << "The bucket " << metadata->name() << " does not have a"
          << " billing configuration. The default applies, i.e., the project"
          << " that owns the bucket pays for the requests.\n";
      return;
    }

    if (metadata->billing().requester_pays) {
      std::cout
          << "The bucket " << metadata->name()
          << " is configured to charge the calling project for the requests.\n";
    } else {
      std::cout << "The bucket " << metadata->name()
                << " is configured to charge the project that owns the bucket "
                   "for the requests.\n";
    }
  }
  //! [get billing] [END storage_get_requester_pays_status]
  (std::move(client), argv.at(0), argv.at(1));
}

void EnableRequesterPays(google::cloud::storage::Client client,
                         std::vector<std::string> const& argv) {
  //! [enable requester pays] [START storage_enable_requester_pays]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetBilling(gcs::BucketBilling{true}));
    if (!metadata) throw std::runtime_error(metadata.status().message());

    std::cout << "Billing configuration for bucket " << metadata->name()
              << " is updated. The bucket now";
    if (!metadata->has_billing()) {
      std::cout << " has no billing configuration.\n";
    } else if (metadata->billing().requester_pays) {
      std::cout << " is configured to charge the caller for requests\n";
    } else {
      std::cout << " is configured to charge the project that owns the bucket"
                << " for requests.\n";
    }
  }
  //! [enable requester pays] [END storage_enable_requester_pays]
  (std::move(client), argv.at(0));
}

void DisableRequesterPays(google::cloud::storage::Client client,
                          std::vector<std::string> const& argv) {
  //! [disable requester pays] [START storage_disable_requester_pays]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& billed_project) {
    StatusOr<gcs::BucketMetadata> metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetBilling(gcs::BucketBilling{false}),
        gcs::UserProject(billed_project));
    if (!metadata) throw std::runtime_error(metadata.status().message());

    std::cout << "Billing configuration for bucket " << bucket_name
              << " is updated. The bucket now";
    if (!metadata->has_billing()) {
      std::cout << " has no billing configuration.\n";
    } else if (metadata->billing().requester_pays) {
      std::cout << " is configured to charge the caller for requests\n";
    } else {
      std::cout << " is configured to charge the project that owns the bucket"
                << " for requests.\n";
    }
  }
  //! [disable requester pays] [END storage_disable_requester_pays]
  (std::move(client), argv.at(0), argv.at(1));
}

void WriteObjectRequesterPays(google::cloud::storage::Client client,
                              std::vector<std::string> const& argv) {
  //! [write object requester pays]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::string const& billed_project) {
    gcs::ObjectWriteStream stream = client.WriteObject(
        bucket_name, object_name, gcs::UserProject(billed_project));
    for (int lineno = 0; lineno != 10; ++lineno) {
      // Add 1 to the counter, because it is conventional to number lines
      // starting at 1.
      stream << (lineno + 1) << ": I will write better examples\n";
    }
    stream.Close();

    StatusOr<gcs::ObjectMetadata> metadata = std::move(stream).metadata();
    if (!metadata) throw std::runtime_error(metadata.status().message());
    std::cout << "Successfully wrote to object " << metadata->name()
              << " its size is: " << metadata->size()
              << "\nFull metadata: " << *metadata << "\n";
  }
  //! [write object requester pays]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void ReadObjectRequesterPays(google::cloud::storage::Client client,
                             std::vector<std::string> const& argv) {
  //! [read object requester pays]
  // [START storage_download_file_requester_pays]
  namespace gcs = ::google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::string const& billed_project) {
    gcs::ObjectReadStream stream = client.ReadObject(
        bucket_name, object_name, gcs::UserProject(billed_project));

    std::string line;
    while (std::getline(stream, line, '\n')) {
      std::cout << line << "\n";
    }
  }
  // [END storage_download_file_requester_pays]
  //! [read object requester pays]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void DeleteObjectRequesterPays(google::cloud::storage::Client client,
                               std::vector<std::string> const& argv) {
  //! [delete object requester pays]
  namespace gcs = ::google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::string const& billed_project) {
    google::cloud::Status status = client.DeleteObject(
        bucket_name, object_name, gcs::UserProject(billed_project));
    if (!status.ok()) throw std::runtime_error(status.message());
  }
  //! [delete object requester pays]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
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
  auto const object_name =
      examples::MakeRandomObjectName(generator, "object-") + ".txt";
  auto client = gcs::Client();

  std::cout << "\nCreating bucket to run the example (" << bucket_name << ")"
            << std::endl;
  (void)client
      .CreateBucketForProject(bucket_name, project_id, gcs::BucketMetadata{})
      .value();
  // In GCS a single project cannot create or delete buckets more often than
  // once every two seconds. We will pause until that time before deleting the
  // bucket.
  auto pause = std::chrono::steady_clock::now() + std::chrono::seconds(2);

  std::cout << "\nRunning GetBilling() example [1]" << std::endl;
  GetBilling(client, {bucket_name, project_id});

  std::cout << "\nRunning EnableRequesterPays() example" << std::endl;
  EnableRequesterPays(client, {bucket_name});

  std::cout << "\nRunning GetBilling() example [2]" << std::endl;
  GetBilling(client, {bucket_name, project_id});

  std::cout << "\nRunning WriteObjectRequesterPays() example" << std::endl;
  WriteObjectRequesterPays(client, {bucket_name, object_name, project_id});

  std::cout << "\nRunning ReadObjectRequesterPays() example" << std::endl;
  ReadObjectRequesterPays(client, {bucket_name, object_name, project_id});

  std::cout << "\nRunning DeleteObjectRequesterPays() example" << std::endl;
  DeleteObjectRequesterPays(client, {bucket_name, object_name, project_id});

  std::cout << "\nRunning DisableRequesterPays() example" << std::endl;
  DisableRequesterPays(client, {bucket_name, project_id});

  std::cout << "\nRunning GetBilling() example [3]" << std::endl;
  GetBilling(client, {bucket_name, project_id});

  if (!examples::UsingEmulator()) std::this_thread::sleep_until(pause);
  (void)examples::RemoveBucketAndContents(client, bucket_name);
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  examples::Example example({
      examples::CreateCommandEntry(
          "get-billing", {"<bucket-name>", "<user-project>"}, GetBilling),
      examples::CreateCommandEntry("enable-requester-pays", {"<bucket-name>"},
                                   EnableRequesterPays),
      examples::CreateCommandEntry("disable-requester-pays",
                                   {"<bucket-name>", "<billed-project>"},
                                   DisableRequesterPays),
      examples::CreateCommandEntry(
          "write-object-requester-pays",
          {"<bucket-name>", "<object-name>", "<billed-project>"},
          WriteObjectRequesterPays),
      examples::CreateCommandEntry(
          "read-object-requester-pays",
          {"<bucket-name>", "<object-name>", "<billed-project>"},
          ReadObjectRequesterPays),
      examples::CreateCommandEntry(
          "delete-object-requester-pays",
          {"<bucket-name>", "<object-name>", "<billed-project>"},
          DeleteObjectRequesterPays),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
