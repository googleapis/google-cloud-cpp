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

#include "google/cloud/bigquery/v2/minimal/internal/project_options.h"
#include "google/cloud/bigquery/v2/minimal/internal/common_options.h"
#include "google/cloud/internal/populate_common_options.h"
#include <chrono>
#include <memory>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

Options ProjectDefaultOptions(Options options) {
  options = google::cloud::internal::PopulateCommonOptions(
      std::move(options), "GOOGLE_CLOUD_CPP_BIGQUERY_V2_PROJECT_ENDPOINT", "",
      "GOOGLE_CLOUD_CPP_BIGQUERY_V2_PROJECT_AUTHORITY",
      "bigquery.googleapis.com");

  if (!options.has<ProjectRetryPolicyOption>()) {
    options.set<ProjectRetryPolicyOption>(
        ProjectLimitedTimeRetryPolicy(std::chrono::minutes(30)).clone());
  }
  if (!options.has<ProjectBackoffPolicyOption>()) {
    options.set<ProjectBackoffPolicyOption>(
        ExponentialBackoffPolicy(std::chrono::seconds(1),
                                 std::chrono::minutes(5), kBackoffScaling)
            .clone());
  }
  if (!options.has<ProjectIdempotencyPolicyOption>()) {
    options.set<ProjectIdempotencyPolicyOption>(
        MakeDefaultProjectIdempotencyPolicy());
  }
  if (!options.has<ProjectConnectionPoolSizeOption>()) {
    options.set<ProjectConnectionPoolSizeOption>(DefaultConnectionPoolSize());
  }

  return options;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
