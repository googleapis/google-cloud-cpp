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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_CLIENT_H

#include "google/cloud/bigquery/v2/minimal/internal/job_connection.h"
#include "google/cloud/options.h"
#include "google/cloud/polling_policy.h"
#include "google/cloud/status_or.h"
#include "google/cloud/stream_range.h"
#include "google/cloud/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// BigQuery Job Client.
///
/// The Job client uses the BigQuery Job API to read Job information from
/// BigQuery.
///
class JobClient {
 public:
  explicit JobClient(std::shared_ptr<BigQueryJobConnection> connection,
                     Options opts = {});
  ~JobClient();

  JobClient(JobClient const&) = default;
  JobClient& operator=(JobClient const&) = default;
  JobClient(JobClient&&) = default;
  JobClient& operator=(JobClient&&) = default;

  friend bool operator==(JobClient const& a, JobClient const& b) {
    return a.connection_ == b.connection_;
  }
  friend bool operator!=(JobClient const& a, JobClient const& b) {
    return !(a == b);
  }

  /**
   * Gets specific job information from Bigquery. For more details on BigQuery
   * jobs, please refer to:
   *
   * https://cloud.google.com/bigquery/docs/jobs-overview
   */
  StatusOr<Job> GetJob(GetJobRequest const& request, Options opts = {});

  /**
   * Lists all jobs that user started in the specified project. Job information
   * is available for a six month period after creation. The job list is sorted
   * in reverse chronological order, by job creation time. Requires the Can
   * View project role, or the Is Owner project role if you set the allUsers
   * property.
   *
   * For more details on BigQuery jobs, please refer to:
   *
   * https://cloud.google.com/bigquery/docs/jobs-overview
   */
  StreamRange<ListFormatJob> ListJobs(ListJobsRequest const& request,
                                      Options opts = {});

  /**
   * Starts a new asynchronous BigQuery job. For more details on BigQuery
   * jobs, please refer to:
   *
   * https://cloud.google.com/bigquery/docs/jobs-overview
   */
  StatusOr<Job> InsertJob(InsertJobRequest const& request, Options opts = {});

  /**
   * Requests that a job be cancelled. This call will return immediately, and
   * the client will need to poll for the job status to see if the cancel
   * completed successfully. Cancelled jobs may still incur costs.
   *
   * For more details on BigQuery jobs, please refer to:
   *
   * https://cloud.google.com/bigquery/docs/jobs-overview
   */
  StatusOr<Job> CancelJob(CancelJobRequest const& request, Options opts = {});

  /**
   * Runs a BigQuery SQL query synchronously and returns query results if the
   * query completes within a specified timeout.
   *
   * For more details on query request fields, please see:
   * https://cloud.google.com/bigquery/docs/reference/rest/v2/jobs/query#request-body
   *
   * For more details on query reqsponse fields, please see:
   * https://cloud.google.com/bigquery/docs/reference/rest/v2/jobs/query#response-body
   */
  StatusOr<PostQueryResults> Query(PostQueryRequest const& request,
                                   Options opts = {});

  /**
   * Gets the result of a Query job.
   *
   * For more details on request fields, please see:
   * https://cloud.google.com/bigquery/docs/reference/rest/v2/jobs/getQueryResults#http-request
   *
   * For more details on reqsponse body, please see:
   * https://cloud.google.com/bigquery/docs/reference/rest/v2/jobs/getQueryResults#response-body
   */
  StatusOr<GetQueryResults> QueryResults(GetQueryResultsRequest const& request,
                                         Options opts = {});

 private:
  std::shared_ptr<BigQueryJobConnection> connection_;
  Options options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_CLIENT_H
