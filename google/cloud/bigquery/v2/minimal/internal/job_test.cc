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

#include "google/cloud/bigquery/v2/minimal/internal/job.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TEST(JobTest, JobDebugString) {
  Job job;
  job.etag = "etag";
  job.id = "1";
  job.kind = "Job";
  job.reference.project_id = "1";
  job.reference.job_id = "2";
  job.status.state = "DONE";
  job.configuration.job_type = "QUERY";
  job.configuration.query_config.query = "select 1;";

  EXPECT_EQ(job.DebugString("Job", TracingOptions{}),
            R"(Job {)"
            R"( etag: "etag")"
            R"( kind: "Job")"
            R"( id: "1")"
            R"( job_configuration: "JobConfiguration {)"
            R"( job_type: "QUERY")"
            R"( query: "select 1;")"
            R"( }")"
            R"( job_reference: "JobReference {)"
            R"( project_id: "1")"
            R"( job_id: "2")"
            R"( location: "")"
            R"( }")"
            R"( job_status: "DONE")"
            R"( error_result: "")"
            R"( })");
}

TEST(JobTest, ListFormatJobDebugString) {
  ListFormatJob job;
  job.id = "1";
  job.kind = "Job";
  job.reference.project_id = "1";
  job.reference.job_id = "2";
  job.state = "DONE";
  job.status.state = "DONE";
  job.configuration.job_type = "QUERY";
  job.configuration.query_config.query = "select 1;";

  EXPECT_EQ(job.DebugString("ListFormatJob", TracingOptions{}),
            R"(ListFormatJob {)"
            R"( id: "1")"
            R"( kind: "Job")"
            R"( state: "DONE")"
            R"( job_configuration: "JobConfiguration {)"
            R"( job_type: "QUERY")"
            R"( query: "select 1;")"
            R"( }")"
            R"( job_reference: "JobReference {)"
            R"( project_id: "1")"
            R"( job_id: "2")"
            R"( location: "")"
            R"( }")"
            R"( job_status: "DONE")"
            R"( error_result: "")"
            R"( })");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
