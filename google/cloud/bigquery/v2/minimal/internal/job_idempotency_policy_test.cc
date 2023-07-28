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

#include "google/cloud/bigquery/v2/minimal/internal/job_idempotency_policy.h"
#include "google/cloud/common_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TEST(JobIdempotencyPolicytTest, GetJob) {
  auto actual = MakeDefaultBigQueryJobIdempotencyPolicy();
  auto expected = Idempotency::kIdempotent;

  GetJobRequest request;
  EXPECT_EQ(actual->GetJob(request), expected);
}

TEST(JobIdempotencyPolicytTest, ListJobs) {
  auto actual = MakeDefaultBigQueryJobIdempotencyPolicy();
  auto expected = Idempotency::kIdempotent;

  ListJobsRequest request;
  EXPECT_EQ(actual->ListJobs(request), expected);
}

TEST(JobIdempotencyPolicytTest, InsertJob) {
  auto actual = MakeDefaultBigQueryJobIdempotencyPolicy();
  auto expected = Idempotency::kNonIdempotent;

  InsertJobRequest request;
  EXPECT_EQ(actual->InsertJob(request), expected);
}

TEST(JobIdempotencyPolicytTest, CancelJob) {
  auto actual = MakeDefaultBigQueryJobIdempotencyPolicy();
  auto expected = Idempotency::kNonIdempotent;

  CancelJobRequest request;
  EXPECT_EQ(actual->CancelJob(request), expected);
}

TEST(JobIdempotencyPolicytTest, QueryNonIdempotent) {
  auto actual = MakeDefaultBigQueryJobIdempotencyPolicy();
  auto expected = Idempotency::kNonIdempotent;

  PostQueryRequest request;
  EXPECT_EQ(actual->Query(request), expected);
}

TEST(JobIdempotencyPolicytTest, QueryIdempotent) {
  auto actual = MakeDefaultBigQueryJobIdempotencyPolicy();
  auto expected = Idempotency::kIdempotent;

  QueryRequest query_request;
  query_request.set_request_id("123");

  PostQueryRequest request;
  request.set_query_request(query_request);

  EXPECT_EQ(actual->Query(request), expected);
}

TEST(JobIdempotencyPolicytTest, GetQueryResults) {
  auto actual = MakeDefaultBigQueryJobIdempotencyPolicy();
  auto expected = Idempotency::kIdempotent;

  GetQueryResultsRequest request;
  EXPECT_EQ(actual->GetQueryResults(request), expected);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
