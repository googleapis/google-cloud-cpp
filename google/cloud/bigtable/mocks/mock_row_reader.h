// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_MOCKS_MOCK_ROW_READER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_MOCKS_MOCK_ROW_READER_H

#include "google/cloud/bigtable/row_reader.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
namespace bigtable_mocks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
// TODO(#8860) - Remove internal namespace.
namespace internal {

/**
 * Returns a `RowReader` with a fixed output stream.
 *
 * This factory function is offered for customers to mock the output of
 * `Table::ReadRows(...)` in their tests.
 *
 * @note If `Cancel()` is called on the `RowReader`, the stream will terminate
 *     and return `final_status`.
 *
 * @param rows a vector containing the `Row`s returned by iterating over the
 *     `RowReader`.
 *
 * @param final_status the final Status of the stream. Defaults to OK.
 *
 * @code
 * TEST(ReadRowsTest, Success) {
 *   using ::google::cloud::StatusOr;
 *   using cbt = ::google::cloud::bigtable;
 *   using cbtm = ::google::cloud::bigtable_mocks;
 *
 *   std::vector<cbt::Row> rows = {cbt::Row("r1", {}), cbt::Row("r2", {})};
 *
 *   auto mock = std::shared_ptr<cbtm::MockDataConnection>();
 *   EXPECT_CALL(*mock, ReadRows)
 *       .WillOnce(Return(cbtm::MakeTestRowReader(rows)));
 *
 *   auto table = cbt::Table(mock);
 *   auto reader = table.ReadRows(...);
 *
 *   // Verify your code works when reading rows: {"r1", "r2"}
 * }
 * @endcode
 */
bigtable::RowReader MakeTestRowReader(std::vector<bigtable::Row> rows,
                                      Status final_status = {});

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_MOCKS_MOCK_ROW_READER_H
