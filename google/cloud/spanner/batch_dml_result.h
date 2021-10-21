// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_BATCH_DML_RESULT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_BATCH_DML_RESULT_H

#include "google/cloud/spanner/version.h"
#include "google/cloud/status.h"
#include <cstdint>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * The result of executing a batch of DML statements.
 *
 * Batch DML statements are executed in order using the
 * `Client::ExecuteBatchDml` method, which accepts a vector of `SqlStatement`
 * objects. The returned `BatchDmlResult` will contain one entry in
 * `BatchDmlResult::stats` for each `SqlStatement` that was executed
 * successfully. If execution of any `SqlStatement` fails, all subsequent
 * statements will not be run and `BatchDmlResult::status` will contain
 * information about the failed statement.
 */
struct BatchDmlResult {
  /// The stats for each successfully executed `SqlStatement`.
  struct Stats {
    /// The number of rows modified by a DML statement.
    std::int64_t row_count;
  };

  /// The stats for each successfully executed `SqlStatement`. The order of
  /// the `SqlStatements` matches the order of the `Stats` in this vector.
  std::vector<Stats> stats;

  /// Either OK or the error Status of the `SqlStatement` that failed.
  Status status;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_BATCH_DML_RESULT_H
