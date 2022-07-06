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

//! [all]

//! [required-includes]
#include "google/cloud/bigtable/mocks/mock_data_connection.h"
#include "google/cloud/bigtable/mocks/mock_row_reader.h"
#include "google/cloud/bigtable/table.h"
#include <gmock/gmock.h>
//! [required-includes]

namespace {

//! [helper-aliases]
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Return;
namespace gc = ::google::cloud;
namespace cbt = ::google::cloud::bigtable;
namespace cbtm = ::google::cloud::bigtable_mocks;
//! [helper-aliases]

TEST(MockTableTest, ReadRowsSuccess) {
  // Create a mock connection:
  //! [create-mock]
  auto mock = std::make_shared<cbtm::MockDataConnection>();
  //! [create-mock]

  // Set up our mock connection to return a `RowReader` that will successfully
  // yield "r1" then "r2":
  //! [simulate-call]
  std::vector<cbt::Row> rows = {cbt::Row("r1", {}), cbt::Row("r2", {})};
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce(Return(ByMove(cbtm::MakeRowReader(rows))));
  //! [simulate-call]

  // Create a table with the mocked connection:
  //! [create-table]
  cbt::Table table(mock, cbt::TableResource("project", "instance", "table"));
  //! [create-table]

  // Make the table call:
  //! [make-call]
  auto reader = table.ReadRows(cbt::RowSet(), cbt::Filter::PassAllFilter());
  //! [make-call]

  // Loop over the rows returned by the `RowReader` and verify the results:
  //! [verify-results]
  std::vector<std::string> row_keys;
  for (gc::StatusOr<cbt::Row> const& row : reader) {
    ASSERT_TRUE(row.ok());
    row_keys.push_back(row->row_key());
  }
  EXPECT_THAT(row_keys, ElementsAre("r1", "r2"));
  //! [verify-results]
}

TEST(MockTableTest, ReadRowsFailure) {
  auto mock = std::make_shared<cbtm::MockDataConnection>();

  // Return a `RowReader` that yields only a failing status (no rows).
  gc::Status final_status(gc::StatusCode::kPermissionDenied, "fail");
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce(Return(ByMove(cbtm::MakeRowReader({}, final_status))));

  cbt::Table table(mock, cbt::TableResource("project", "instance", "table"));
  cbt::RowReader reader =
      table.ReadRows(cbt::RowSet(), cbt::Filter::PassAllFilter());

  // In this test, we expect one `StatusOr<Row>`, that holds a bad status.
  auto it = reader.begin();
  ASSERT_NE(it, reader.end());
  EXPECT_FALSE((*it).ok());
  ASSERT_EQ(++it, reader.end());
}

TEST(TableTest, AsyncReadRows) {
  // Let's use an alias to ignore fields we don't care about.
  using ::testing::Unused;

  // Create a mock connection, and set its expectations.
  auto mock = std::make_shared<cbtm::MockDataConnection>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, auto const& on_row, auto const& on_finish, Unused,
                   Unused, Unused) {
        // Simulate returning two rows, "r1" and "r2", by invoking the `on_row`
        // callback. Verify the values of the returned `future<bool>`s.
        EXPECT_TRUE(on_row(cbt::Row("r1", {})).get());
        EXPECT_TRUE(on_row(cbt::Row("r2", {})).get());
        // Simulate a stream that ends successfully.
        on_finish(gc::Status());
      });

  // Create the table with a mocked connection.
  cbt::Table table(mock, cbt::TableResource("project", "instance", "table"));

  // These are example callbacks for demonstration purposes. Applications should
  // likely invoke their own callbacks when testing.
  auto on_row = [](cbt::Row const&) { return gc::make_ready_future(true); };
  auto on_finish = [](gc::Status const&) {};

  // Make the client call.
  table.AsyncReadRows(on_row, on_finish, cbt::RowSet(),
                      cbt::Filter::PassAllFilter());
}

}  // namespace

//! [all]l
