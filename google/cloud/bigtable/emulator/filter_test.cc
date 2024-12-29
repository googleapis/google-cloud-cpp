// Copyright 2024 Google LLC
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

#include "google/cloud/testing_util/is_proto_equal.h"
#include <gmock/gmock.h>
#include "google/cloud/bigtable/data_connection.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/testing_util/status_matchers.h"

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {
namespace {

TEST(DummyFilter, Simple) {
  google::cloud::bigtable::Table table(MakeDataConnection(),
                                       TableResource("fake", "baz", "ft"));
  Filter filter =
      Filter::Chain(Filter::FamilyRegex("fam1"), Filter::CellsRowOffset(2));
  for (StatusOr<Row>& row :
       table.ReadRows(RowSet(RowRange::InfiniteRange()), filter)) {
    ASSERT_STATUS_OK(row);
    std::cout << row->row_key() << ":\n";
    for (auto const& cell : row->cells()) {
      std::cout << "\t" << cell.family_name() << ":" << cell.column_qualifier()
                << "    @ " << cell.timestamp().count() << "us\n"
                << "\t\"" << cell.value() << '"' << "\n";
    }
  }
}

}  // anonymous namespace
}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

