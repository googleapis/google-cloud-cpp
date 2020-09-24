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
#include <sstream>
#include <thread>

namespace {

void CreateSignedPolicyDocumentV2(google::cloud::storage::Client client,
                                  std::vector<std::string> const& argv) {
  //! [create signed policy document]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::PolicyDocumentResult> document =
        client.CreateSignedPolicyDocument(gcs::PolicyDocument{
            std::chrono::system_clock::now() + std::chrono::minutes(15),
            {
                gcs::PolicyDocumentCondition::StartsWith("key", ""),
                gcs::PolicyDocumentCondition::ExactMatchObject(
                    "acl", "bucket-owner-read"),
                gcs::PolicyDocumentCondition::ExactMatchObject(
                    "bucket", std::move(bucket_name)),
                gcs::PolicyDocumentCondition::ExactMatch("Content-Type",
                                                         "image/jpeg"),
                gcs::PolicyDocumentCondition::ContentLengthRange(0, 1000000),
            }});
    if (!document) throw std::runtime_error(document.status().message());

    std::cout << "The signed document is: " << *document << "\n\n"
              << "You can use this with an HTML form.\n";
  }
  //! [create signed policy document]
  (std::move(client), argv.at(0));
}

void CreateSignedPolicyDocumentV4(google::cloud::storage::Client client,
                                  std::vector<std::string> const& argv) {
  //! [create signed policy document v4]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::PolicyDocumentV4Result> document =
        client.GenerateSignedPostPolicyV4(gcs::PolicyDocumentV4{
            std::move(bucket_name),
            "scan_0001.jpg",
            std::chrono::minutes(15),
            std::chrono::system_clock::now(),
            {
                gcs::PolicyDocumentCondition::StartsWith("key", ""),
                gcs::PolicyDocumentCondition::ExactMatchObject(
                    "acl", "bucket-owner-read"),
                gcs::PolicyDocumentCondition::ExactMatch("Content-Type",
                                                         "image/jpeg"),
                gcs::PolicyDocumentCondition::ContentLengthRange(0, 1000000),
            }});
    if (!document) throw std::runtime_error(document.status().message());

    std::cout << "The signed document is: " << *document << "\n\n"
              << "You can use this with an HTML form.\n";
  }
  //! [create signed policy document v4]
  (std::move(client), argv.at(0));
}

void CreatePolicyDocumentFormV4(google::cloud::storage::Client client,
                                std::vector<std::string> const& argv) {
  // [START storage_generate_signed_post_policy_v4]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
    auto document = client.GenerateSignedPostPolicyV4(
        gcs::PolicyDocumentV4{
            bucket_name,
            object_name,
            /*expiration=*/std::chrono::minutes(10),
        },
        gcs::AddExtensionFieldOption("x-goog-meta-test", "data"));
    if (!document) throw std::runtime_error(document.status().message());

    // Create the HTML form for the computed policy.
    std::ostringstream os;
    os << "<form action='" << document->url << "' method='POST'"
       << " enctype='multipart/form-data'>\n";
    for (auto const& field : document->required_form_fields) {
      os << "  <input name='" << field.first << "' value='" << field.second
         << "' type='hidden' />\n";
    }
    os << "  <input type='submit' value='Upload File' name='submit' /><br />\n"
       << "  <input type='file' name='file' /><br />\n"
       << "</form>";

    std::cout << "A sample HTML form:\n" << os.str() << "\n";
  }
  // [END storage_generate_signed_post_policy_v4]
  (std::move(client), argv.at(0), argv.at(1));
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
      examples::MakeRandomObjectName(generator, "upload-object-");
  auto client = gcs::Client::CreateDefaultClient().value();
  std::cout << "\nCreating bucket to run the example (" << bucket_name << ")"
            << std::endl;
  (void)client
      .CreateBucketForProject(bucket_name, project_id, gcs::BucketMetadata{})
      .value();
  // In GCS a single project cannot create or delete buckets more often than
  // once every two seconds. We will pause until that time before deleting the
  // bucket.
  auto pause = std::chrono::steady_clock::now() + std::chrono::seconds(2);

  std::cout << "\nRunning the CreatedSignedPolicyDocumentV2() example"
            << std::endl;
  CreateSignedPolicyDocumentV2(client, {bucket_name});

  std::cout << "\nRunning the CreatedSignedPolicyDocumentV4() example"
            << std::endl;
  CreateSignedPolicyDocumentV4(client, {bucket_name});

  std::cout << "\nRunning the CreatePolicyDocumentFormV4() example"
            << std::endl;
  CreatePolicyDocumentFormV4(client, {bucket_name, object_name});

  if (!examples::UsingTestbench()) std::this_thread::sleep_until(pause);
  (void)examples::RemoveBucketAndContents(client, bucket_name);
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  examples::Example example({
      examples::CreateCommandEntry("create-signed-policy-document-v2",
                                   {"<bucket-name>"},
                                   CreateSignedPolicyDocumentV2),
      examples::CreateCommandEntry("create-signed-policy-document-v4",
                                   {"<bucket-name>"},
                                   CreateSignedPolicyDocumentV4),
      examples::CreateCommandEntry("create-policy-document-form-v4",
                                   {"<bucket-name>", "<object-name>"},
                                   CreatePolicyDocumentFormV4),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
