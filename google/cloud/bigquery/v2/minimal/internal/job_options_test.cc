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

#include "google/cloud/bigquery/v2/minimal/internal/job_options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TEST(JobOptionstTest, DefaultOptions) {
  Options opts;
  auto actual = BigQueryJobDefaultOptions(std::move(opts));
  auto expected_retry =
      BigQueryJobLimitedTimeRetryPolicy(std::chrono::minutes(30)).clone();
  auto expected_backoff = ExponentialBackoffPolicy(
      std::chrono::seconds(1), std::chrono::minutes(5), 2.0);
  auto expected_idempotency = Idempotency::kIdempotent;
  auto const* default_endpoint = "bigquery.googleapis.com";

  EXPECT_EQ(actual.get<EndpointOption>(), default_endpoint);
  EXPECT_EQ(actual.get<AuthorityOption>(), default_endpoint);

  GetJobRequest request;
  EXPECT_TRUE(actual.has<BigQueryJobIdempotencyPolicyOption>());
  EXPECT_EQ(actual.get<BigQueryJobIdempotencyPolicyOption>()->GetJob(request),
            expected_idempotency);

  EXPECT_TRUE(actual.has<BigQueryJobRetryPolicyOption>());
  EXPECT_EQ(actual.get<BigQueryJobRetryPolicyOption>()->IsExhausted(),
            expected_retry->IsExhausted());

  EXPECT_TRUE(actual.has<BigQueryJobBackoffPolicyOption>());

  EXPECT_GT(actual.get<BigQueryJobConnectionPoolSizeOption>(), 0);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
