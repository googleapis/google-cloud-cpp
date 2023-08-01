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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_TESTING_JOB_QUERY_TEST_UTILS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_TESTING_JOB_QUERY_TEST_UTILS_H

#include "google/cloud/bigquery/v2/minimal/internal/job_request.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_response.h"
#include "google/cloud/log.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

bigquery_v2_minimal_internal::QueryRequest MakeQueryRequest();
bigquery_v2_minimal_internal::PostQueryRequest MakePostQueryRequest();
bigquery_v2_minimal_internal::GetQueryResultsRequest
MakeFullGetQueryResultsRequest();

void AssertEquals(bigquery_v2_minimal_internal::QueryRequest const& lhs,
                  bigquery_v2_minimal_internal::QueryRequest const& rhs);

void AssertEquals(bigquery_v2_minimal_internal::PostQueryRequest const& lhs,
                  bigquery_v2_minimal_internal::PostQueryRequest const& rhs);

std::string MakeQueryResponsePayload();
bigquery_v2_minimal_internal::PostQueryResults MakePostQueryResults();

void AssertEquals(bigquery_v2_minimal_internal::PostQueryResults const& lhs,
                  bigquery_v2_minimal_internal::PostQueryResults const& rhs);

std::string MakeGetQueryResultsResponsePayload();
bigquery_v2_minimal_internal::GetQueryResults MakeGetQueryResults();
void AssertEquals(bigquery_v2_minimal_internal::GetQueryResults const& lhs,
                  bigquery_v2_minimal_internal::GetQueryResults const& rhs);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_TESTING_JOB_QUERY_TEST_UTILS_H
