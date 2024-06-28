// Copyright 2024 Google LLC
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

//! [START bigqueryjob_quickstart] [all]
#include "google/cloud/bigquery/job/v2/job_client.h"
#include <iostream>

namespace {}  // namespace

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <project-id>\n";
    return 1;
  }

  std::string const project_id = std::string(argv[1]);

  namespace bigquery_proto = google::cloud::bigquery::v2;
  namespace bigquery_job = google::cloud::bigquery_job_v2;
  auto client = bigquery_job::JobServiceClient(
      bigquery_job::MakeJobServiceConnectionRest());

  bigquery_proto::ListJobsRequest list_request;
  list_request.set_project_id(project_id);
  std::vector<std::string> job_ids;
  for (auto job : client.ListJobs(list_request)) {
    if (!job) throw std::move(job).status();
    std::cout << job->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
//! [END bigqueryjob_quickstart] [all]
