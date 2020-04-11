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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/examples/storage_examples_common.h"
#include "google/cloud/internal/getenv.h"
#include <iostream>

namespace {

void GetStaticWebsiteConfiguration(google::cloud::storage::Client client,
                                   std::vector<std::string> const& argv) {
  //! [print bucket website configuration]
  // [START storage_print_bucket_website_configuration]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name);

    if (!bucket_metadata) {
      throw std::runtime_error(bucket_metadata.status().message());
    }

    if (!bucket_metadata->has_website()) {
      std::cout << "Static website configuration is not set for bucket "
                << bucket_metadata->name() << "\n";
      return;
    }

    std::cout << "Static website configuration set for bucket "
              << bucket_metadata->name() << "\nThe main page suffix is: "
              << bucket_metadata->website().main_page_suffix
              << "\nThe not found page is: "
              << bucket_metadata->website().not_found_page << "\n";
  }
  // [END storage_print_bucket_website_configuration]
  //! [print bucket website configuration]
  (std::move(client), argv.at(0));
}

void SetStaticWebsiteConfiguration(google::cloud::storage::Client client,
                                   std::vector<std::string> const& argv) {
  //! [define bucket website configuration]
  // [START storage_define_bucket_website_configuration]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string main_page_suffix,
     std::string not_found_page) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) throw std::runtime_error(original.status().message());
    StatusOr<gcs::BucketMetadata> patched_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetWebsite(
            gcs::BucketWebsite{main_page_suffix, not_found_page}),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!patched_metadata) {
      throw std::runtime_error(patched_metadata.status().message());
    }

    if (!patched_metadata->has_website()) {
      std::cout << "Static website configuration is not set for bucket "
                << patched_metadata->name() << "\n";
      return;
    }

    std::cout << "Static website configuration successfully set for bucket "
              << patched_metadata->name() << "\nNew main page suffix is: "
              << patched_metadata->website().main_page_suffix
              << "\nNew not found page is: "
              << patched_metadata->website().not_found_page << "\n";
  }
  // [END storage_define_bucket_website_configuration]
  //! [define bucket website configuration]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void RemoveStaticWebsiteConfiguration(google::cloud::storage::Client client,
                                      std::vector<std::string> const& argv) {
  //! [remove bucket website configuration]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) throw std::runtime_error(original.status().message());
    StatusOr<gcs::BucketMetadata> patched_metadata = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().ResetWebsite(),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!patched_metadata) {
      throw std::runtime_error(patched_metadata.status().message());
    }

    if (!patched_metadata->has_website()) {
      std::cout << "Static website configuration removed for bucket "
                << patched_metadata->name() << "\n";
      return;
    }

    std::cout << "Static website configuration is set for bucket "
              << patched_metadata->name()
              << "\nThis is unexpected, and may indicate that another"
              << " application has modified the bucket concurrently.\n";
  }
  //! [remove bucket website configuration]
  (std::move(client), argv.at(0));
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
  auto const bucket_name =
      examples::MakeRandomBucketName(generator, "cloud-cpp-test-examples-");
  auto client = gcs::Client::CreateDefaultClient().value();
  std::cout << "\nCreating bucket to run the example (" << bucket_name << ")"
            << std::endl;
  auto bucket_metadata = client
                             .CreateBucketForProject(bucket_name, project_id,
                                                     gcs::BucketMetadata{})
                             .value();

  std::cout << "\nRunning SetStaticWebsiteConfiguration() example" << std::endl;
  SetStaticWebsiteConfiguration(
      client, {bucket_name, "main-page.html", "not-found.html"});

  std::cout << "\nRunning GetStaticWebsiteConfiguration() example" << std::endl;
  GetStaticWebsiteConfiguration(client, {bucket_name});

  std::cout << "\nRunning RemoveStaticWebsiteConfiguration() example"
            << std::endl;
  RemoveStaticWebsiteConfiguration(client, {bucket_name});

  (void)client.DeleteBucket(bucket_name);
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  auto make_entry = [](std::string const& name,
                       std::vector<std::string> arg_names,
                       examples::ClientCommand const& cmd) {
    arg_names.insert(arg_names.begin(), "<bucket-name>");
    return examples::CreateCommandEntry(name, arg_names, cmd);
  };
  google::cloud::storage::examples::Example example({
      make_entry("get-static-website-configuration", {},
                 GetStaticWebsiteConfiguration),
      make_entry("set-static-website-configuration",
                 {"<main-page-suffix>", "<not-found-page>"},
                 SetStaticWebsiteConfiguration),
      make_entry("remove-static-website-configuration", {},
                 RemoveStaticWebsiteConfiguration),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
