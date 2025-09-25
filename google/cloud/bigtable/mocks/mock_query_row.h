// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_MOCKS_MOCK_QUERY_ROW_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_MOCKS_MOCK_QUERY_ROW_H

#include "google/cloud/bigtable/query_row.h"
#include "google/cloud/bigtable/value.h"
#include "google/cloud/version.h"
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_mocks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Creates a `bigtable::QueryRow` with the specified column names and values.
 *
 * This overload accepts a vector of pairs, allowing the caller to specify both
 * the column names and the `bigtable::Value` that goes in each column.
 *
 * This function is intended for application developers who are mocking the
 * results of a `Client::ReadRows` call.
 */
inline bigtable::QueryRow MakeQueryRow(
    std::vector<std::pair<std::string, bigtable::Value>> const& columns) {
  auto names = std::make_shared<std::vector<std::string>>();
  std::vector<bigtable::Value> values;
  for (auto const& c : columns) {
    names->push_back(c.first);
    values.push_back(c.second);
  }
  return bigtable_internal::QueryRowFriend::MakeQueryRow(values, names);
}

/**
 * Creates a `bigtable::QueryRow` with `bigtable::Value`s created from the given
 * arguments and with auto-generated column names.
 *
 * This overload accepts a variadic list of arguments that will be used to
 * create the `bigtable::Value`s in the row. The column names will be implicitly
 * generated, the first column being "0", the second "1", and so on,
 * corresponding to the argument's position.
 *
 * This function is intended for application developers who are mocking the
 * results of a `Client::ReadRows` call.
 */
template <typename... Ts>
bigtable::QueryRow MakeQueryRow(Ts&&... values) {
  auto names = std::make_shared<std::vector<std::string>>();
  int i = 0;
  for (auto const& v : {bigtable::Value(values)...}) {
    (void)v;
    names->push_back(std::to_string(i++));
  }
  return bigtable_internal::QueryRowFriend::MakeQueryRow(
      {bigtable::Value(values)...}, names);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_MOCKS_MOCK_QUERY_ROW_H
