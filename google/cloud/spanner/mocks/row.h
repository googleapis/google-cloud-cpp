// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_MOCKS_ROW_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_MOCKS_ROW_H

#include "google/cloud/spanner/row.h"
#include "google/cloud/spanner/value.h"
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace spanner_mocks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Creates a `spanner::Row` with the specified column names and values.
 *
 * This overload accepts a vector of pairs, allowing the caller to specify both
 * the column names and the `spanner::Value` that goes in each column.
 *
 * This function is intended for application developers who are mocking the
 * results of a `Client::ExecuteQuery` call.
 */
inline spanner::Row MakeRow(
    std::vector<std::pair<std::string, spanner::Value>> pairs) {
  return spanner::MakeTestRow(std::move(pairs));
}

/**
 * Creates a `spanner::Row` with `spanner::Value`s created from the given
 * arguments and with auto-generated column names.
 *
 * This overload accepts a variadic list of arguments that will be used to
 * create the `spanner::Value`s in the row. The column names will be implicitly
 * generated, the first column being "0", the second "1", and so on,
 * corresponding to the argument's position.
 *
 * This function is intended for application developers who are mocking the
 * results of a `Client::ExecuteQuery` call.
 */
template <typename... Ts>
spanner::Row MakeRow(Ts&&... ts) {
  return spanner::MakeTestRow(std::forward<Ts>(ts)...);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_MOCKS_ROW_H
