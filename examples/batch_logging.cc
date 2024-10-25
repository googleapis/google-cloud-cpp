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
// [START batch_job_logs]
#include "google/cloud/batch/v1/batch_client.h"
#include "google/cloud/logging/v2/logging_service_v2_client.h"
#include "google/cloud/location.h"
#include "google/cloud/project.h"

// [END batch_job_logs]
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

void JobLogs(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw google::cloud::testing_util::Usage{
        "job-logs <project-id> <location-id> <job-id>"};
  }
  // [START batch_job_logs]
  [](std::string const& project_id, std::string const& location_id,
     std::string const& job_id) {
    auto const project = google::cloud::Project(project_id);
    auto const location = google::cloud::Location(project, location_id);
    auto const name = location.FullName() + "/jobs/" + job_id;
    auto batch = google::cloud::batch_v1::BatchServiceClient(
        google::cloud::batch_v1::MakeBatchServiceConnection());
    auto job = batch.GetJob(name);
    if (!job) throw std::move(job).status();

    auto logging = google::cloud::logging_v2::LoggingServiceV2Client(
        google::cloud::logging_v2::MakeLoggingServiceV2Connection());
    auto const log_name = project.FullName() + "/logs/batch_task_logs";
    google::logging::v2::ListLogEntriesRequest request;
    request.mutable_resource_names()->Add(project.FullName());
    request.set_filter("logName=\"" + log_name +
                       "\" labels.job_uid=" + job->uid());
    for (auto l : logging.ListLogEntries(request)) {
      if (!l) throw std::move(l).status();
      std::cout << l->text_payload() << "\n";
    }
  }
  // [END batch_job_logs]
  (argv.at(0), argv.at(1), argv.at(2));
}

// Use the same value as in google/cloud/batch/samples/samples.cc so only one
// of these samples needs to cleanup stale jobs.
auto constexpr kJobPrefix = "batch-examples-";

google::cloud::batch::v1::Job CreateTestJob(
    google::cloud::batch_v1::BatchServiceClient client,
    std::string const& project_id, std::string const& location_id,
    std::string const& job_id) {
  google::cloud::batch::v1::CreateJobRequest request;
  request.set_parent(
      google::cloud::Location(google::cloud::Project(project_id), location_id)
          .FullName());
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
  return client.CreateJob(request).value();
}

void WaitForJob(google::cloud::batch_v1::BatchServiceClient client,
                std::string const& job_name) {
  std::cout << "\nWaiting for " << job_name << std::flush;
  // It takes about 60 seconds to finish a job, so waiting for about 5 minutes
  // seems enough.
  auto const polling_period = std::chrono::seconds(10);
  for (int i = 0; i != 30; ++i) {
    auto response = client.GetJob(job_name);
    auto done = [](auto state) {
      using google::cloud::batch::v1::JobStatus;
      return state == JobStatus::SUCCEEDED || state == JobStatus::FAILED;
    };
    if (response && done(response->status().state())) {
      std::cout << ".DONE\n";
      return;
    }
    std::cout << "." << std::flush;
    std::this_thread::sleep_for(polling_period);
  }
  std::cout << ".DONE (TIMEOUT)\n";
  throw std::runtime_error("Timeout waiting for job");
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  using ::google::cloud::internal::GetEnv;
  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_TEST_REGION",
  });
  auto const project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const location_id = GetEnv("GOOGLE_CLOUD_CPP_TEST_REGION").value();

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const job_id =
      kJobPrefix + google::cloud::internal::Sample(
                       generator, 32, "abcdefghijklmnopqrstuvwxyz");

  auto client = google::cloud::batch_v1::BatchServiceClient(
      google::cloud::batch_v1::MakeBatchServiceConnection());

  // Create the job to drive this test.
  auto job = CreateTestJob(client, project_id, location_id, job_id);
  std::cout << "Created test job: " << job.name() << "\n";

  // Wait until the job completes, otherwise the logs may be empty. The logs may
  // still be delayed, but this is less likely.
  WaitForJob(client, job.name());

  std::cout << "Running JobLogs() test" << std::endl;
  JobLogs({project_id, location_id, job_id});

  (void)client.DeleteJob(job.name()).get().value();
  std::cout << "Deleted test job: " << job.name() << "\n";
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  google::cloud::testing_util::Example example({
      {"job-logs", JobLogs},
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
