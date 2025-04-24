// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storagebatchoperations/v1/storage_batch_operations_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/example_driver.h"
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

std::string GetParentFromEnv() {
  google::cloud::testing_util::CheckEnvironmentVariablesAreSet(
      {"GOOGLE_CLOUD_PROJECT", "GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID"});
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const location_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID")
          .value();
  return "projects/" + project_id + "/locations" + location_id;
}

void CreateJob(
    google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
        client,
    std::vector<std::string> const& argv) {
  //! [storage_batch_create_job]
  [](google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
         client,
     std::string const& job_id) {
    auto const parent = GetParentFromEnv();
    namespace sbo = google::cloud::storagebatchoperations::v1;
    sbo::Job job;
    auto result = client.CreateJob(parent, job, job_id).get();
    if (!result) throw result.status();
    std::cout << "Created job: " << result->name() << "\n";
  }
  //! [storage_batch_create_job]
  (std::move(client), argv.at(0));
}

void ListJobs(
    google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
        client,
    std::vector<std::string> const& argv) {
  //! [storage_batch_list_jobs]
  [](google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
         client) {
    auto const parent = GetParentFromEnv();
    for (auto const& job : client.ListJobs(parent)) {
      if (!job) throw job.status();
      std::cout << job->name() << "\n";
    }
  }
  //! [storage_batch_list_jobs]
  (std::move(client));
}

void GetJob(
    google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
        client,
    std::vector<std::string> const& argv) {
  //! [storage_batch_get_job]
  [](google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
         client,
     std::string const& job_id) {
    auto const parent = GetParentFromEnv();
    auto const name = parent + "/jobs/" + job_id;
    auto job = client.GetJob(name);
    if (!job) throw job.status();
    std::cout << "Got job: " << job->name() << "\n";
  }
  //! [storage_batch_get_job]
  (std::move(client), argv.at(0));
}

void CancelJob(
    google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
        client,
    std::vector<std::string> const& argv) {
  //! [storage_batch_cancel_job]
  [](google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
         client,
     std::string const& job_id) {
    auto const parent = GetParentFromEnv();
    auto const name = parent + "/jobs/" + job_id;
    auto response = client.CancelJob(name);
    if (!response) throw response.status();
    std::cout << "Cancelled job: " << name << "\n";
  }
  //! [storage_batch_cancel_job]
  (std::move(client), argv.at(0));
}

void DeleteJob(
    google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
        client,
    std::vector<std::string> const& argv) {
  //! [storage_batch_delete_job]
  [](google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
         client,
     std::string const& job_id) {
    auto const parent = GetParentFromEnv();
    auto const name = parent + "/jobs/" + job_id;
    auto status = client.DeleteJob(name);
    if (!status.ok()) throw status;
    std::cout << "Deleted job: " << name << "\n";
  }
  //! [storage_batch_delete_job]
  (std::move(client), argv.at(0));
}

void AutoRun(std::vector<std::string> const& argv) {
  if (!argv.empty()) throw google::cloud::testing_util::Usage{"auto"};
  google::cloud::testing_util::CheckEnvironmentVariablesAreSet(
      {"GOOGLE_CLOUD_PROJECT", "GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID"});
  auto const parent = GetParentFromEnv();

  auto gen = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const prefix = std::string{"storage-batch-operations-samples"};
  auto const job_id =
      prefix + "_" +
      google::cloud::internal::Sample(gen, 32, "abcdefghijklmnopqrstuvwxyz");

  auto client =
      google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient(
          google::cloud::storagebatchoperations_v1::
              MakeStorageBatchOperationsConnection());

  std::cout << "\nRunning CreateJob() example\n";
  CreateJob(client, {job_id});

  std::cout << "\nRunning GetJob() example\n";
  GetJob(client, {job_id});

  std::cout << "\nRunning ListJobs() example\n";
  ListJobs(client, {});

  std::cout << "\nRunning CancelJob() example\n";
  CancelJob(client, {job_id});

  std::cout << "\nRunning DeleteJob() example\n";
  DeleteJob(client, {job_id});
}

}  // namespace

int main(int argc, char* argv[]) {
  using google::cloud::testing_util::Example;
  namespace sbo = google::cloud::storagebatchoperations_v1;
  auto make_entry = [](std::string name,
                       std::vector<std::string> const& arg_names,
                       std::function<void(sbo::StorageBatchOperationsClient,
                                          std::vector<std::string>)>
                           cmd) {
    auto adapter = [=](std::vector<std::string> argv) {
      if ((argv.size() == 1 && argv[0] == "--help") ||
          argv.size() != arg_names.size()) {
        std::string usage = name;
        for (auto const& a : arg_names) usage += " <" + a + ">";
        throw google::cloud::testing_util::Usage{std::move(usage)};
      }
      auto client = sbo::StorageBatchOperationsClient(
          sbo::MakeStorageBatchOperationsConnection());
      cmd(client, std::move(argv));
    };
    return google::cloud::testing_util::Commands::value_type(
        std::move(name), std::move(adapter));
  };
  Example example({
      make_entry("create-job", {"job-id"}, CreateJob),
      make_entry("get-job", {"job-id"}, GetJob),
      make_entry("list-jobs", {}, ListJobs),
      make_entry("cancel-job", {"job-id"}, CancelJob),
      make_entry("delete-job", {"job-id"}, DeleteJob),
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
