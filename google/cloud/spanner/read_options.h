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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_READ_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_READ_OPTIONS_H

#include "google/cloud/spanner/version.h"
#include <google/spanner/v1/spanner.pb.h>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/// Options passed to `Client::Read` or `Client::PartitionRead`.
struct ReadOptions {
  /**
   * If non-empty, the name of an index on a database table. This index is used
   * instead of the table primary key when interpreting the `KeySet`and sorting
   * result rows.
   */
  std::string index_name;

  /**
   * Limit on the number of rows to yield, or 0 for no limit.
   * A limit cannot be specified when calling `PartitionRead`.
   */
  std::int64_t limit = 0;
};

inline bool operator==(ReadOptions const& lhs, ReadOptions const& rhs) {
  return lhs.limit == rhs.limit && lhs.index_name == rhs.index_name;
}

inline bool operator!=(ReadOptions const& lhs, ReadOptions const& rhs) {
  return !(lhs == rhs);
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_READ_OPTIONS_H
