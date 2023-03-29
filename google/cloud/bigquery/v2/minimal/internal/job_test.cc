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

  std::string expected =
      "Job{etag=etag, kind=Job, id=1, "
      "job_configuration={job_type=QUERY, query=select 1;}, "
      "job_reference={job_id=2, location=, project_id=1}, job_status=DONE, "
      "error_result=}";
  std::string truncated =
      "Job{etag=etag, kind=Job, id=1, "
      "job_configuration={job_type=QUERY, "
      "query=select 1;}, job_reference={job_id=2, location=, "
      "project_...<truncated>...";

  EXPECT_EQ(expected, job.DebugString(TracingOptions{}.SetOptions(
                          "truncate_string_field_longer_than=1024")));
  EXPECT_EQ(truncated, job.DebugString(TracingOptions{}));
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

  std::string expected =
      "ListFormatJob{id=1, kind=Job, state=DONE, "
      "job_configuration={job_type=QUERY, query=select 1;}, "
      "job_reference={job_id=2, location=, project_id=1}, job_status=DONE, "
      "error_result=}";

  std::string truncated =
      "ListFormatJob{id=1, kind=Job, state=DONE, "
      "job_configuration={job_type=QUERY, query=select 1;}, "
      "job_reference={job_id=2, location...<truncated>...";

  EXPECT_EQ(expected, job.DebugString(TracingOptions{}.SetOptions(
                          "truncate_string_field_longer_than=1024")));
  EXPECT_EQ(truncated, job.DebugString(TracingOptions{}));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
