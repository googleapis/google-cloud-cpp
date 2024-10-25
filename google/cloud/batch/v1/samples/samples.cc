// Copyright 2023 Google LLC
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

// The #includes (with the extra blank line) are part of the code extracted into
// the reference documentation. We want to highlight what includes are needed.
// [START batch_create_script_job]
// [START batch_create_container_job]
// [START batch_create_script_job_with_bucket]
// [START batch_create_job_with_template]
// [START batch_get_job]
// [START batch_get_task]
// [START batch_list_jobs]
// [START batch_list_tasks]
// [START batch_delete_job]
#include "google/cloud/batch/v1/batch_client.h"

// [END batch_delete_job]
// [END batch_list_tasks]
// [END batch_list_jobs]
// [END batch_get_task]
// [END batch_get_job]
// [END batch_create_job_with_template]
// [END batch_create_script_job_with_bucket]
// [END batch_create_container_job]
// [END batch_create_script_job]
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/example_driver.h"
#include "absl/strings/match.h"
#include "google/protobuf/text_format.h"
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

void CreateContainerJob(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw google::cloud::testing_util::Usage{
        "create-container-job <project-id> <location-id> <job-id>"};
  }
  // [START batch_create_container_job]
  [](std::string const& project_id, std::string const& location_id,
     std::string const& job_id) {
    // Initialize the request; start with the fields that depend on the sample
    // input.
    google::cloud::batch::v1::CreateJobRequest request;
    request.set_parent("projects/" + project_id + "/locations/" + location_id);
    request.set_job_id(job_id);
    // Most of the job description is fixed in this example; use a string to
    // initialize it.
    auto constexpr kText = R"pb(
      task_groups {
        task_count: 4
        task_spec {
          compute_resource { cpu_milli: 500 memory_mib: 16 }
          max_retry_count: 2
          max_run_duration { seconds: 3600 }
          runnables {
            container {
              image_uri: "gcr.io/google-containers/busybox"
              entrypoint: "/bin/sh"
              commands: "-c"
              commands: "echo Hello world! This is task ${BATCH_TASK_INDEX}. This job has a total of ${BATCH_TASK_COUNT} tasks."
            }
          }
        }
      }
      allocation_policy {
        instances {
          policy { machine_type: "e2-standard-4" provisioning_model: STANDARD }
        }
      }
      labels { key: "env" value: "testing" }
      labels { key: "type" value: "container" }
      logs_policy { destination: CLOUD_LOGGING }
    )pb";
    auto* job = request.mutable_job();
    if (!google::protobuf::TextFormat::ParseFromString(kText, job)) {
      throw std::runtime_error("Error parsing Job description");
    }
    // Create a client and issue the request.
    auto client = google::cloud::batch_v1::BatchServiceClient(
        google::cloud::batch_v1::MakeBatchServiceConnection());
    auto response = client.CreateJob(request);
    if (!response) throw std::move(response).status();
    std::cout << "Job : " << response->DebugString() << "\n";
  }
  // [END batch_create_container_job]
  (argv.at(0), argv.at(1), argv.at(2));
}

void CreateScriptJob(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw google::cloud::testing_util::Usage{
        "create-script-job <project-id> <location-id> <job-id>"};
  }
  // [START batch_create_script_job]
  [](std::string const& project_id, std::string const& location_id,
     std::string const& job_id) {
    // Initialize the request; start with the fields that depend on the sample
    // input.
    google::cloud::batch::v1::CreateJobRequest request;
    request.set_parent("projects/" + project_id + "/locations/" + location_id);
    request.set_job_id(job_id);
    // Most of the job description is fixed in this example; use a string to
    // initialize it.
    auto constexpr kText = R"pb(
      task_groups {
        task_count: 4
        task_spec {
          compute_resource { cpu_milli: 500 memory_mib: 16 }
          max_retry_count: 2
          max_run_duration { seconds: 3600 }
          runnables {
            script {
              text: "echo Hello world! This is task ${BATCH_TASK_INDEX}. This job has a total of ${BATCH_TASK_COUNT} tasks."
            }
          }
        }
      }
      allocation_policy {
        instances {
          policy { machine_type: "e2-standard-4" provisioning_model: STANDARD }
        }
      }
      labels { key: "env" value: "testing" }
      labels { key: "type" value: "script" }
      logs_policy { destination: CLOUD_LOGGING }
    )pb";
    auto* job = request.mutable_job();
    if (!google::protobuf::TextFormat::ParseFromString(kText, job)) {
      throw std::runtime_error("Error parsing Job description");
    }
    // Create a client and issue the request.
    auto client = google::cloud::batch_v1::BatchServiceClient(
        google::cloud::batch_v1::MakeBatchServiceConnection());
    auto response = client.CreateJob(request);
    if (!response) throw std::move(response).status();
    std::cout << "Job : " << response->DebugString() << "\n";
  }
  // [END batch_create_script_job]
  (argv.at(0), argv.at(1), argv.at(2));
}

void CreateScriptJobWithBucket(std::vector<std::string> const& argv) {
  if (argv.size() != 4) {
    throw google::cloud::testing_util::Usage{
        "create-script-job-with-bucket <project-id> <location-id> <job-id> "
        "<bucket-name>"};
  }
  // [START batch_create_script_job_with_bucket]
  [](std::string const& project_id, std::string const& location_id,
     std::string const& job_id, std::string const& bucket_name) {
    // Initialize the request; start with the fields that depend on the sample
    // input.
    google::cloud::batch::v1::CreateJobRequest request;
    request.set_parent("projects/" + project_id + "/locations/" + location_id);
    request.set_job_id(job_id);
    // Most of the job description is fixed in this example; use a string to
    // initialize it, and then override the GCS remote path.
    auto constexpr kText = R"pb(
      task_groups {
        task_count: 4
        task_spec {
          compute_resource { cpu_milli: 500 memory_mib: 16 }
          max_retry_count: 2
          max_run_duration { seconds: 3600 }
          runnables {
            script {
              text: "echo Hello world from task ${BATCH_TASK_INDEX}. >> /mnt/share/output_task_${BATCH_TASK_INDEX}.txt"
            }
          }
          volumes { mount_path: "/mnt/share" }
        }
      }
      allocation_policy {
        instances {
          policy { machine_type: "e2-standard-4" provisioning_model: STANDARD }
        }
      }
      labels { key: "env" value: "testing" }
      labels { key: "type" value: "script" }
      logs_policy { destination: CLOUD_LOGGING }
    )pb";
    auto* job = request.mutable_job();
    if (!google::protobuf::TextFormat::ParseFromString(kText, job)) {
      throw std::runtime_error("Error parsing Job description");
    }
    job->mutable_task_groups(0)
        ->mutable_task_spec()
        ->mutable_volumes(0)
        ->mutable_gcs()
        ->set_remote_path(bucket_name);
    // Create a client and issue the request.
    auto client = google::cloud::batch_v1::BatchServiceClient(
        google::cloud::batch_v1::MakeBatchServiceConnection());
    auto response = client.CreateJob(request);
    if (!response) throw std::move(response).status();
    std::cout << "Job : " << response->DebugString() << "\n";
  }
  // [END batch_create_script_job_with_bucket]
  (argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void CreateJobWithTemplate(std::vector<std::string> const& argv) {
  if (argv.size() != 4) {
    throw google::cloud::testing_util::Usage{
        "create-job-with-template <project-id> <location-id> <job-id> "
        "<template-name>"};
  }
  // [START batch_create_job_with_template]
  [](std::string const& project_id, std::string const& location_id,
     std::string const& job_id, std::string const& template_name) {
    // Initialize the request; start with the fields that depend on the sample
    // input.
    google::cloud::batch::v1::CreateJobRequest request;
    request.set_parent("projects/" + project_id + "/locations/" + location_id);
    request.set_job_id(job_id);
    // Most of the job description is fixed in this example; use a string to
    // initialize it, and then override the template name.
    auto constexpr kText = R"pb(
      task_groups {
        task_count: 4
        task_spec {
          compute_resource { cpu_milli: 500 memory_mib: 16 }
          max_retry_count: 2
          max_run_duration { seconds: 3600 }
          runnables {
            script {
              text: "echo Hello world! This is task ${BATCH_TASK_INDEX}. This job has a total of ${BATCH_TASK_COUNT} tasks."
            }
          }
        }
      }
      labels { key: "env" value: "testing" }
      labels { key: "type" value: "script" }
      logs_policy { destination: CLOUD_LOGGING }
    )pb";
    auto* job = request.mutable_job();
    if (!google::protobuf::TextFormat::ParseFromString(kText, job)) {
      throw std::runtime_error("Error parsing Job description");
    }
    job->mutable_allocation_policy()->add_instances()->set_instance_template(
        template_name);
    // Create a client and issue the request.
    auto client = google::cloud::batch_v1::BatchServiceClient(
        google::cloud::batch_v1::MakeBatchServiceConnection());
    auto response = client.CreateJob(request);
    if (!response) throw std::move(response).status();
    std::cout << "Job : " << response->DebugString() << "\n";
  }
  // [END batch_create_job_with_template]
  (argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void GetJob(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw google::cloud::testing_util::Usage{
        "get-job <project-id> <location-id> <job-id>"};
  }
  // [START batch_get_job]
  [](std::string const& project_id, std::string const& location_id,
     std::string const& job_id) {
    auto const name = "projects/" + project_id + "/locations/" + location_id +
                      "/jobs/" + job_id;
    // Initialize a client and issue the request.
    auto client = google::cloud::batch_v1::BatchServiceClient(
        google::cloud::batch_v1::MakeBatchServiceConnection());
    auto response = client.GetJob(name);
    if (!response) throw std::move(response).status();
    std::cout << "GetJob() succeeded with " << response->DebugString() << "\n";
  }
  // [END batch_get_job]
  (argv.at(0), argv.at(1), argv.at(2));
}

void GetTask(std::vector<std::string> const& argv) {
  if (argv.size() != 5) {
    throw google::cloud::testing_util::Usage{
        "get-task <project-id> <location-id> <job-id> <group-id> "
        "<task-number>"};
  }
  // [START batch_get_task]
  [](std::string const& project_id, std::string const& location_id,
     std::string const& job_id, std::string const& group_id,
     std::string const& task_number) {
    auto const name = "projects/" + project_id + "/locations/" + location_id +
                      "/jobs/" + job_id + "/taskGroups/" + group_id +
                      "/tasks/" + task_number;
    // Initialize a client and issue the request.
    auto client = google::cloud::batch_v1::BatchServiceClient(
        google::cloud::batch_v1::MakeBatchServiceConnection());
    auto response = client.GetTask(name);
    if (!response) throw std::move(response).status();
    std::cout << "GetTask() succeeded with " << response->DebugString() << "\n";
  }
  // [END batch_get_task]
  (argv.at(0), argv.at(1), argv.at(2), argv.at(3), argv.at(4));
}

void ListJobs(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw google::cloud::testing_util::Usage{
        "list-jobs <project-id> <location-id>"};
  }
  // [START batch_list_jobs]
  [](std::string const& project_id, std::string const& location_id) {
    auto const parent = "projects/" + project_id + "/locations/" + location_id;
    // Initialize a client and issue the request.
    auto client = google::cloud::batch_v1::BatchServiceClient(
        google::cloud::batch_v1::MakeBatchServiceConnection());
    int i = 0;
    for (auto job : client.ListJobs(parent)) {
      if (!job) throw std::move(job).status();
      std::cout << "Job[" << i++ << "]  " << job->DebugString() << "\n";
    }
  }
  // [END batch_list_jobs]
  (argv.at(0), argv.at(1));
}

void ListTasks(std::vector<std::string> const& argv) {
  if (argv.size() != 4) {
    throw google::cloud::testing_util::Usage{
        "list-tasks <project-id> <location-id> <job-id> <group-id>"};
  }
  // [START batch_list_tasks]
  [](std::string const& project_id, std::string const& location_id,
     std::string const& job_id, std::string const& group_id) {
    auto const parent = "projects/" + project_id + "/locations/" + location_id +
                        "/jobs/" + job_id + "/taskGroups/" + group_id;
    // Initialize a client and issue the request.
    auto client = google::cloud::batch_v1::BatchServiceClient(
        google::cloud::batch_v1::MakeBatchServiceConnection());
    int i = 0;
    for (auto task : client.ListTasks(parent)) {
      if (!task) throw std::move(task).status();
      std::cout << "Task[" << i++ << "]  " << task->DebugString() << "\n";
    }
  }
  // [END batch_list_tasks]
  (argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void DeleteJob(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw google::cloud::testing_util::Usage{
        "create-script-job <project-id> <location-id> <job-id>"};
  }
  // [START batch_delete_job]
  [](std::string const& project_id, std::string const& location_id,
     std::string const& job_id) {
    auto const name = "projects/" + project_id + "/locations/" + location_id +
                      "/jobs/" + job_id;
    google::cloud::batch::v1::DeleteJobRequest request;
    request.set_name(name);
    // Initialize a client and issue the request.
    auto client = google::cloud::batch_v1::BatchServiceClient(
        google::cloud::batch_v1::MakeBatchServiceConnection());
    auto future = client.DeleteJob(request);
    // Wait until the long-running operation completes.
    auto success = future.get();
    if (!success) throw std::move(success).status();
    std::cout << "Job " << name << " successfully deleted\n";
  }
  // [END batch_delete_job]
  (argv.at(0), argv.at(1), argv.at(2));
}

auto constexpr kJobPrefix = "batch-examples-";

void CleanupStaleJobs(std::string const& project_id,
                      std::string const& location_id) {
  auto client = google::cloud::batch_v1::BatchServiceClient(
      google::cloud::batch_v1::MakeBatchServiceConnection());
  auto const parent = "projects/" + project_id + "/locations/" + location_id;
  auto const prefix = parent + "/jobs/" + kJobPrefix;
  auto const stale_threshold =
      std::chrono::system_clock::now() - std::chrono::hours(48);
  for (auto const& job : client.ListJobs(parent)) {
    if (!job) return;
    auto const create_time =
        google::cloud::internal::ToChronoTimePoint(job->create_time());
    if (!absl::StartsWith(job->name(), prefix) ||
        create_time >= stale_threshold) {
      continue;
    }
    google::cloud::batch::v1::DeleteJobRequest request;
    request.set_name(job->name());
    // We expect that 10 seconds is enough to create the LRO and poll it. If
    // the LRO was not created in 10 seconds, then the next run of this
    // program will have another chance to clean up. If it is created, but
    // does not complete in 10 seconds, then it is fine for the LRO to
    // continue running while this program does other things.
    (void)client.DeleteJob(request).wait_for(std::chrono::seconds(10));
  }
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  using ::google::cloud::internal::GetEnv;
  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_TEST_REGION",
      "GOOGLE_CLOUD_CPP_BATCH_TEST_TEMPLATE_NAME",
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME",
  });
  auto const project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const location_id = GetEnv("GOOGLE_CLOUD_CPP_TEST_REGION").value();
  auto const template_name =
      GetEnv("GOOGLE_CLOUD_CPP_BATCH_TEST_TEMPLATE_NAME").value();
  auto const bucket_name =
      GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value();
  CleanupStaleJobs(project_id, location_id);

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const container_job_id =
      kJobPrefix + google::cloud::internal::Sample(
                       generator, 32, "abcdefghijklmnopqrstuvwxyz");
  auto const script_job_id =
      kJobPrefix + google::cloud::internal::Sample(
                       generator, 32, "abcdefghijklmnopqrstuvwxyz");
  auto const script_with_bucket_job_id =
      kJobPrefix + google::cloud::internal::Sample(
                       generator, 32, "abcdefghijklmnopqrstuvwxyz");
  auto const script_with_template_job_id =
      kJobPrefix + google::cloud::internal::Sample(
                       generator, 32, "abcdefghijklmnopqrstuvwxyz");

  // We launch this job first, and do as much work as possible before blocking
  // to wait for it.
  std::cout << "\nRunning CreateScriptJob() example" << std::endl;
  CreateScriptJob({project_id, location_id, script_job_id});

  std::cout << "\nRunning CreateScriptJobWithBucket() example" << std::endl;
  CreateScriptJobWithBucket(
      {project_id, location_id, script_with_bucket_job_id, bucket_name});

  std::cout << "\nRunning CreateJobWithTemplate() example" << std::endl;
  CreateJobWithTemplate(
      {project_id, location_id, script_with_template_job_id, template_name});

  std::cout << "\nRunning CreateContainerJob() example" << std::endl;
  CreateContainerJob({project_id, location_id, container_job_id});

  std::cout << "\nRunning GetJob() example" << std::endl;
  GetJob({project_id, location_id, container_job_id});

  std::cout << "\nRunning ListJobs() example" << std::endl;
  ListJobs({project_id, location_id});

  std::cout << "\nRunning DeleteJob() example [1]" << std::endl;
  DeleteJob({project_id, location_id, container_job_id});

  std::cout << "\nRunning DeleteJob() example [2]" << std::endl;
  DeleteJob({project_id, location_id, script_with_bucket_job_id});

  std::cout << "\nRunning DeleteJob() example [3]" << std::endl;
  DeleteJob({project_id, location_id, script_with_template_job_id});

  // We delay GetTask() until the job completes.
  auto client = google::cloud::batch_v1::BatchServiceClient(
      google::cloud::batch_v1::MakeBatchServiceConnection());

  std::cout << "\nWaiting for " << script_job_id << std::flush;
  bool success = false;
  auto const name = "projects/" + project_id + "/locations/" + location_id +
                    "/jobs/" + script_job_id;
  // It takes about 60 seconds to finish a job, so waiting for about 5 minutes
  // seems enough.
  auto const polling_period = std::chrono::seconds(10);
  for (int i = 0; i != 30; ++i) {
    auto response = client.GetJob(name);
    auto done = [](auto state) {
      using google::cloud::batch::v1::JobStatus;
      return state == JobStatus::SUCCEEDED || state == JobStatus::FAILED;
    };
    if (response && done(response->status().state())) {
      success = true;
      break;
    }
    std::cout << "." << std::flush;
    std::this_thread::sleep_for(polling_period);
  }
  std::cout << ".DONE\n";
  if (success) {
    std::cout << "\nRunning GetTask() example" << std::endl;
    GetTask({project_id, location_id, script_job_id, "group0", "0"});

    std::cout << "\nRunning ListTasks() example" << std::endl;
    ListTasks({project_id, location_id, script_job_id, "group0"});
  }

  std::cout << "\nRunning DeleteJob() example [3]" << std::endl;
  DeleteJob({project_id, location_id, script_job_id});
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  google::cloud::testing_util::Example example({
      {"create-container-job", CreateContainerJob},
      {"create-script-job", CreateScriptJob},
      {"create-script-job-with-bucket", CreateScriptJobWithBucket},
      {"create-job-with-template", CreateJobWithTemplate},
      {"get-job", GetJob},
      {"get-task", GetTask},
      {"list-jobs", ListJobs},
      {"list-tasks", ListTasks},
      {"delete-job", DeleteJob},
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
