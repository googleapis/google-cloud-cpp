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

void CreateJob(
    google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
        client,
    std::vector<std::string> const& argv) {
  //! [storage_batch_create_job]
  [](google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
         client,
     std::string const& project_id, std::string const& job_id,
     std::string const& target_bucket_name, std::string const& object_prefix) {
    auto const parent =
        std::string{"projects/"} + project_id + "/locations/global";
    namespace sbo = google::cloud::storagebatchoperations::v1;
    sbo::Job job;
    sbo::BucketList* bucket_list = job.mutable_bucket_list();
    sbo::BucketList::Bucket* bucket_config = bucket_list->add_buckets();
    bucket_config->set_bucket(target_bucket_name);
    sbo::PrefixList* prefix_list_config = bucket_config->mutable_prefix_list();
    prefix_list_config->add_included_object_prefixes(object_prefix);
    sbo::DeleteObject* delete_object_config = job.mutable_delete_object();
    delete_object_config->set_permanent_object_deletion_enabled(false);
    auto result = client.CreateJob(parent, job, job_id).get();
    if (!result) throw result.status();
    std::cout << "Created job: " << result->name() << "\n";
  }
  //! [storage_batch_create_job]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void ListJobs(
    google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
        client,
    std::vector<std::string> const& argv) {
  //! [storage_batch_list_jobs]
  [](google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
         client,
     std::string const& project_id) {
    auto const parent =
        std::string{"projects/"} + project_id + "/locations/global";
    for (auto const& job : client.ListJobs(parent)) {
      if (!job) throw job.status();
      std::cout << job->name() << "\n";
    }
  }
  //! [storage_batch_list_jobs]
  (std::move(client), argv.at(0));
}

void GetJob(
    google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
        client,
    std::vector<std::string> const& argv) {
  //! [storage_batch_get_job]
  [](google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
         client,
     std::string const& project_id, std::string const& job_id) {
    auto const parent =
        std::string{"projects/"} + project_id + "/locations/global";
    auto const name = parent + "/jobs/" + job_id;
    auto job = client.GetJob(name);
    if (!job) throw job.status();
    std::cout << "Got job: " << job->name() << "\n";
  }
  //! [storage_batch_get_job]
  (std::move(client), argv.at(0), argv.at(1));
}

void CancelJob(
    google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
        client,
    std::vector<std::string> const& argv) {
  //! [storage_batch_cancel_job]
  [](google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
         client,
     std::string const& project_id, std::string const& job_id) {
    auto const parent =
        std::string{"projects/"} + project_id + "/locations/global";
    auto const name = parent + "/jobs/" + job_id;
    auto response = client.CancelJob(name);
    if (!response) throw response.status();
    std::cout << "Cancelled job: " << name << "\n";
  }
  //! [storage_batch_cancel_job]
  (std::move(client), argv.at(0), argv.at(1));
}

void DeleteJob(
    google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
        client,
    std::vector<std::string> const& argv) {
  //! [storage_batch_delete_job]
  [](google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient
         client,
     std::string const& project_id, std::string const& job_id) {
    auto const parent =
        std::string{"projects/"} + project_id + "/locations/global";
    auto const name = parent + "/jobs/" + job_id;
    auto status = client.DeleteJob(name);
    if (!status.ok()) throw status;
    std::cout << "Deleted job: " << name << "\n";
  }
  //! [storage_batch_delete_job]
  (std::move(client), argv.at(0), argv.at(1));
}

void AutoRun(std::vector<std::string> const& argv) {
  if (!argv.empty()) throw google::cloud::testing_util::Usage{"auto"};
  google::cloud::testing_util::CheckEnvironmentVariablesAreSet(
      {"GOOGLE_CLOUD_PROJECT", "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME"});
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const target_bucket_name =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
          .value();

  auto gen = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const prefix = std::string{"storage-batch-operations-samples"};
  auto const alphanumeric = "abcdefghijklmnopqrstuvwxyz0123456789";
  auto const job_id =
      prefix + "-" + google::cloud::internal::Sample(gen, 32, alphanumeric);

  std::string object_prefix = "sbo-test-objects/";

  auto client =
      google::cloud::storagebatchoperations_v1::StorageBatchOperationsClient(
          google::cloud::storagebatchoperations_v1::
              MakeStorageBatchOperationsConnection());

  std::cout << "\nRunning CreateJob() example\n";
  CreateJob(client, {project_id, job_id, target_bucket_name, object_prefix});

  std::cout << "\nRunning GetJob() example\n";
  GetJob(client, {project_id, job_id});

  std::cout << "\nRunning ListJobs() example\n";
  ListJobs(client, {project_id});

  std::cout << "\nRunning CancelJob() example\n";
  try {
    CancelJob(client, {project_id, job_id});
  } catch (google::cloud::Status const& ex) {
    std::cerr << "INFO: CancelJob threw: " << ex.message()
              << " (This might be expected if the job completed quickly or "
                 "failed creation)"
              << std::endl;
  }

  std::cout << "\nRunning DeleteJob() example\n";
  DeleteJob(client, {project_id, job_id});
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
      make_entry(
          "create-job",
          {"project-id", "job-id", "target-bucket-name", "object-prefix"},
          CreateJob),
      make_entry("get-job", {"project-id", "job-id"}, GetJob),
      make_entry("list-jobs", {"project-id"}, ListJobs),
      make_entry("cancel-job", {"project-id", "job-id"}, CancelJob),
      make_entry("delete-job", {"project-id", "job-id"}, DeleteJob),
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
