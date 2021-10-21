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

void CreateGetSignedUrlV4(google::cloud::storage::Client client,
                          std::vector<std::string> const& argv) {
  //! [sign url v4] [START storage_generate_signed_url_v4]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::string const& signing_account) {
    StatusOr<std::string> signed_url = client.CreateV4SignedUrl(
        "GET", std::move(bucket_name), std::move(object_name),
        gcs::SignedUrlDuration(std::chrono::minutes(15)),
        gcs::SigningAccount(signing_account));

    if (!signed_url) throw std::runtime_error(signed_url.status().message());
    std::cout << "The signed url is: " << *signed_url << "\n\n"
              << "You can use this URL with any user agent, for example:\n"
              << "curl '" << *signed_url << "'\n";
  }
  //! [sign url v4] [END storage_generate_signed_url_v4]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void CreatePutSignedUrlV4(google::cloud::storage::Client client,
                          std::vector<std::string> const& argv) {
  //! [create put signed url v4] [START storage_generate_upload_signed_url_v4]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::string const& signing_account) {
    StatusOr<std::string> signed_url = client.CreateV4SignedUrl(
        "PUT", std::move(bucket_name), std::move(object_name),
        gcs::SignedUrlDuration(std::chrono::minutes(15)),
        gcs::AddExtensionHeader("content-type", "application/octet-stream"),
        gcs::SigningAccount(signing_account));

    if (!signed_url) throw std::runtime_error(signed_url.status().message());
    std::cout << "The signed url is: " << *signed_url << "\n\n"
              << "You can use this URL with any user agent, for example:\n"
              << "curl -X PUT -H 'Content-Type: application/octet-stream'"
              << " --upload-file my-file '" << *signed_url << "'\n";
  }
  //! [create put signed url v4] [END storage_generate_upload_signed_url_v4]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  namespace gcs = ::google::cloud::storage;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME",
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const bucket_name = google::cloud::internal::GetEnv(
                               "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                               .value();
  auto const signing_account =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT")
          .value();
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const object_name =
      examples::MakeRandomObjectName(generator, "cloud-cpp-test-examples-");

  auto client = gcs::Client();

  if (examples::UsingEmulator()) {
    std::cout << "Signed URL examples are only runnable against production\n";
    return;
  }

  std::cout << "\nRunning CreatePutSignedUrlV4() example" << std::endl;
  CreatePutSignedUrlV4(client, {bucket_name, object_name, signing_account});

  std::cout << "\nRunning CreateGetSignedUrlV4() example" << std::endl;
  CreateGetSignedUrlV4(client, {bucket_name, object_name, signing_account});
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  auto make_entry = [](std::string const& name,
                       examples::ClientCommand const& cmd) {
    return examples::CreateCommandEntry(
        name, {"<bucket-name>", "<object-name>", "<signing-account>"}, cmd);
  };
  examples::Example example({
      make_entry("create-get-signed-url-v4", CreateGetSignedUrlV4),
      make_entry("create-put-signed-url-v4", CreatePutSignedUrlV4),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
