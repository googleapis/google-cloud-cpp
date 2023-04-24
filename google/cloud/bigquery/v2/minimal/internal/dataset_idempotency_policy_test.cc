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

#include "google/cloud/bigquery/v2/minimal/internal/dataset_idempotency_policy.h"
#include "google/cloud/common_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TEST(DatasetIdempotencyPolicytTest, GetDataset) {
  auto actual = MakeDefaultDatasetIdempotencyPolicy();
  auto expected = Idempotency::kIdempotent;

  GetDatasetRequest request;
  EXPECT_EQ(actual->GetDataset(request), expected);
}

TEST(DatasetIdempotencyPolicytTest, ListDatasets) {
  auto actual = MakeDefaultDatasetIdempotencyPolicy();
  auto expected = Idempotency::kIdempotent;

  ListDatasetsRequest request;
  EXPECT_EQ(actual->ListDatasets(request), expected);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
