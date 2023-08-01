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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_MOCKS_MOCK_JOB_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_MOCKS_MOCK_JOB_CONNECTION_H

#include "google/cloud/bigquery/v2/minimal/internal/job_connection.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/*
 * A class to mock `BigQueryJobConnection`.
 *
 * Application developers may want to test their code with simulated responses,
 * including errors, from an object of type `BigQueryJobClient`. To do so,
 * construct an object of type `BigQueryJobClient` with an instance of this
 * class. Then use the Google Test framework functions to program the behavior
 * of this mock.
 */
// TODO(#11039): Move to "bigquery_v2_minimal_mocks" namespace when api is ready
// to go public.
class MockBigQueryJobConnection : public BigQueryJobConnection {
 public:
  MOCK_METHOD(Options, options, (), (override));

  MOCK_METHOD(StatusOr<Job>, GetJob, (GetJobRequest const& request),
              (override));

  MOCK_METHOD(StreamRange<ListFormatJob>, ListJobs,
              (ListJobsRequest const& request), (override));

  MOCK_METHOD(StatusOr<Job>, InsertJob, (InsertJobRequest const& request),
              (override));

  MOCK_METHOD(StatusOr<Job>, CancelJob, (CancelJobRequest const& request),
              (override));

  MOCK_METHOD(StatusOr<PostQueryResults>, Query,
              (PostQueryRequest const& request), (override));

  MOCK_METHOD(StatusOr<GetQueryResults>, QueryResults,
              (GetQueryResultsRequest const& request), (override));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_MOCKS_MOCK_JOB_CONNECTION_H
