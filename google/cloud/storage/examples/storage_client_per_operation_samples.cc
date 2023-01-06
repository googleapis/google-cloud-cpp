// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
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
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include <iostream>
#include <string>
#include <thread>

namespace {

void ChangeUserAgent(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  if ((argv.size() == 1 && argv[0] == "--help") || argv.size() != 3) {
    throw examples::Usage{
        "change-user-agent"
        " <bucket-name> <object-name-1> <object-name-2>"};
  }
  //! [change-user-agent]
  namespace g = ::google::cloud;
  namespace gcs = ::google::cloud::storage;
  [](std::string const& bucket_name, std::string const& object_name_1,
     std::string const& object_name_2) {
    auto client = gcs::Client();
    auto metadata =
        client.InsertObject(bucket_name, object_name_1,
                            "The quick brown fox jumps over the lazy dog",
                            g::Options{}.set<g::UserAgentProductsOption>(
                                {"example", "InsertObject"}));
    if (!metadata) throw std::runtime_error(metadata.status().message());

    auto is = client.ReadObject(bucket_name, object_name_1,
                                g::Options{}.set<g::UserAgentProductsOption>(
                                    {"example", "ReadObject"}));
    auto contents = std::string{std::istreambuf_iterator<char>(is.rdbuf()), {}};
    if (is.bad()) throw std::runtime_error(is.status().message());

    auto os = client.WriteObject(bucket_name, object_name_2,
                                 g::Options{}.set<g::UserAgentProductsOption>(
                                     {"example", "WriteObject"}));
    os << contents;
    os.Close();
    if (os.bad()) throw std::runtime_error(os.metadata().status().message());

    auto result = client.DeleteObject(
        bucket_name, object_name_1, gcs::Generation(metadata->generation()),
        g::Options{}.set<g::UserAgentProductsOption>(
            {"example", "DeleteObject"}));
    if (!result.ok()) throw std::runtime_error(metadata.status().message());

    metadata = os.metadata();
    result = client.DeleteObject(bucket_name, object_name_2,
                                 gcs::Generation(metadata->generation()),
                                 g::Options{}.set<g::UserAgentProductsOption>(
                                     {"example", "DeleteObject"}));
    if (!result.ok()) throw std::runtime_error(metadata.status().message());
  }
  //! [change-user-agent]
  (argv.at(0), argv.at(1), argv.at(2));
}

void ChangeRetryPolicy(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  if ((argv.size() == 1 && argv[0] == "--help") || argv.size() != 3) {
    throw examples::Usage{
        "change-retry-policy"
        " <bucket-name> <object-name-1> <object-name-2>"};
  }
  //! [change-retry-policy]
  namespace g = ::google::cloud;
  namespace gcs = ::google::cloud::storage;
  [](std::string const& bucket_name, std::string const& object_name_1,
     std::string const& object_name_2) {
    auto client = gcs::Client();
    auto metadata = client.InsertObject(
        bucket_name, object_name_1,
        "The quick brown fox jumps over the lazy dog",
        g::Options{}.set<gcs::RetryPolicyOption>(
            gcs::LimitedTimeRetryPolicy(std::chrono::seconds(10)).clone()));
    if (!metadata) throw std::runtime_error(metadata.status().message());

    auto is = client.ReadObject(
        bucket_name, object_name_1,
        g::Options{}.set<gcs::RetryPolicyOption>(
            gcs::LimitedTimeRetryPolicy(std::chrono::seconds(10)).clone()));
    auto contents = std::string{std::istreambuf_iterator<char>(is.rdbuf()), {}};
    if (is.bad()) throw std::runtime_error(is.status().message());

    auto os = client.WriteObject(
        bucket_name, object_name_2,
        g::Options{}.set<gcs::RetryPolicyOption>(
            gcs::LimitedTimeRetryPolicy(std::chrono::seconds(10)).clone()));
    os << contents;
    os.Close();
    if (os.bad()) throw std::runtime_error(os.metadata().status().message());

    auto result = client.DeleteObject(
        bucket_name, object_name_1, gcs::Generation(metadata->generation()),
        g::Options{}.set<gcs::RetryPolicyOption>(
            gcs::LimitedTimeRetryPolicy(std::chrono::seconds(10)).clone()));
    if (!result.ok()) throw std::runtime_error(metadata.status().message());

    metadata = os.metadata();
    result = client.DeleteObject(
        bucket_name, object_name_2, gcs::Generation(metadata->generation()),
        g::Options{}.set<gcs::RetryPolicyOption>(
            gcs::LimitedTimeRetryPolicy(std::chrono::seconds(10)).clone()));
    if (!result.ok()) throw std::runtime_error(metadata.status().message());
  }
  //! [change-retry-policy]
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
  auto client = gcs::Client();
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

  std::cout << "\nRunning ChangeUserAgent()" << std::endl;
  auto const object_name_1 =
      examples::MakeRandomObjectName(generator, "object-");
  auto const object_name_2 =
      examples::MakeRandomObjectName(generator, "object-");
  ChangeUserAgent({bucket_name, object_name_1, object_name_2});

  std::cout << "\nRunning ChangeRetryPolicy()" << std::endl;
  auto const object_name_3 =
      examples::MakeRandomObjectName(generator, "object-");
  auto const object_name_4 =
      examples::MakeRandomObjectName(generator, "object-");
  ChangeRetryPolicy({bucket_name, object_name_3, object_name_4});

  if (!examples::UsingEmulator()) std::this_thread::sleep_until(delete_after);
  (void)examples::RemoveBucketAndContents(client, bucket_name);
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  examples::Example example({
      {"change-user-agent", ChangeUserAgent},
      {"change-retry-policy", ChangeRetryPolicy},
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
