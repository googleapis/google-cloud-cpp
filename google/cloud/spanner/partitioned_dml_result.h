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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_PARTITIONED_DML_RESULT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_PARTITIONED_DML_RESULT_H

#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/spanner/version.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * The result of executing a Partitioned DML query.
 */
struct PartitionedDmlResult {
  /// A lower bound on the number of rows modified.
  std::int64_t row_count_lower_bound;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_PARTITIONED_DML_RESULT_H
