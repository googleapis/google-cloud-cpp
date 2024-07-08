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

//! [all]
#include "google/cloud/bigquerycontrol/job/v2/job_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  auto const project_id = argv[1];

  namespace bigquerycontrol = ::google::cloud::bigquerycontrol_job_v2;
  namespace bigquery_v2_proto = ::google::cloud::bigquery::v2;
  auto client = bigquerycontrol::JobServiceClient(
      bigquerycontrol::MakeJobServiceConnectionRest());

  bigquery_v2_proto::ListJobsRequest list_request;
  list_request.set_project_id(project_id);

  for (auto r : client.ListJobs(list_request)) {
    if (!r) throw std::move(r).status();
    std::cout << r->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
//! [all]
