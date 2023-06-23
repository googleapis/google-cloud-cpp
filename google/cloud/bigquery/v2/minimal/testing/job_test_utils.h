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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_TESTING_JOB_TEST_UTILS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_TESTING_JOB_TEST_UTILS_H

#include "google/cloud/bigquery/v2/minimal/internal/job.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_query_stats.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_stats.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

bigquery_v2_minimal_internal::JobConfiguration MakeJobConfiguration();
bigquery_v2_minimal_internal::JobConfigurationQuery MakeJobConfigurationQuery();

bigquery_v2_minimal_internal::JobQueryStatistics MakeJobQueryStats();
void AssertEquals(bigquery_v2_minimal_internal::JobQueryStatistics& expected,
                  bigquery_v2_minimal_internal::JobQueryStatistics& actual);

bigquery_v2_minimal_internal::JobStatistics MakeJobStats();
void AssertEquals(bigquery_v2_minimal_internal::JobStatistics& expected,
                  bigquery_v2_minimal_internal::JobStatistics& actual);

bigquery_v2_minimal_internal::Job MakeJob();
bigquery_v2_minimal_internal::Job MakePartialJob();
void AssertEqualsPartial(bigquery_v2_minimal_internal::Job& expected,
                         bigquery_v2_minimal_internal::Job& actual);
void AssertEquals(bigquery_v2_minimal_internal::Job& expected,
                  bigquery_v2_minimal_internal::Job& actual);

bigquery_v2_minimal_internal::ListFormatJob MakeListFormatJob();
void AssertEquals(bigquery_v2_minimal_internal::ListFormatJob& expected,
                  bigquery_v2_minimal_internal::ListFormatJob& actual);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_TESTING_JOB_TEST_UTILS_H
