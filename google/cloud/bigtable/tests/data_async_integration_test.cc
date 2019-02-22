// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/testing/table_integration_test.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/init_google_mock.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {
class DataAsyncIntegrationTest
    : public bigtable::testing::TableIntegrationTest {
 protected:
  noex::Table GetNoExTable() {
    return noex::Table(data_client_,
                       bigtable::testing::TableTestEnvironment::table_id());
  }
};

using namespace google::cloud::testing_util::chrono_literals;

TEST_F(DataAsyncIntegrationTest, TableApply) {
  auto sync_table = GetTable();
  auto table = GetNoExTable();

  std::string const row_key = "row-key-1";
  std::vector<bigtable::Cell> created{
      {row_key, "family1", "c0", 1000, "v1000"},
      {row_key, "family1", "c1", 2000, "v2000"}};
  SingleRowMutation mut(row_key);
  for (auto const& c : created) {
    mut.emplace_back(SetCell(
        c.family_name(), c.column_qualifier(),
        std::chrono::duration_cast<std::chrono::milliseconds>(c.timestamp()),
        c.value()));
  }

  CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  std::promise<void> done;
  table.AsyncApply(
      cq,
      [&done](CompletionQueue& cq, google::bigtable::v2::MutateRowResponse& r,
              grpc::Status const& status) {
        done.set_value();
        EXPECT_TRUE(status.ok());
      },
      std::move(mut));

  // Block until the asynchronous operation completes. This is not what one
  // would do in a real application (the synchronous API is better in that
  // case), but we need to wait before checking the results.
  done.get_future().get();

  // Validate that the newly created cells are actually in the server.
  std::vector<bigtable::Cell> expected{
      {row_key, "family1", "c0", 1000, "v1000"},
      {row_key, "family1", "c1", 2000, "v2000"}};

  auto actual = ReadRows(sync_table, bigtable::Filter::PassAllFilter());

  // Cleanup the thread running the completion queue event loop.
  cq.Shutdown();
  pool.join();
  CheckEqualUnordered(expected, actual);
}

TEST_F(DataAsyncIntegrationTest, TableBulkApply) {
  auto sync_table = GetTable();
  auto table = GetNoExTable();

  std::string const row_key1 = "row-key-1";
  std::string const row_key2 = "row-key-2";
  std::map<std::string, std::vector<bigtable::Cell>> created{
      {row_key1,
       {{row_key1, "family1", "c0", 1000, "v1000"},
        {row_key1, "family1", "c1", 2000, "v2000"}}},
      {row_key2,
       {{row_key2, "family1", "c0", 3000, "v1000"},
        {row_key2, "family1", "c0", 4000, "v1000"}}}};

  BulkMutation mut;
  for (auto const& row_cells : created) {
    auto const& row_key = row_cells.first;
    auto const& cells = row_cells.second;
    SingleRowMutation row_mut(row_key);
    for (auto const& c : cells) {
      row_mut.emplace_back(SetCell(
          c.family_name(), c.column_qualifier(),
          std::chrono::duration_cast<std::chrono::milliseconds>(c.timestamp()),
          c.value()));
    }
    mut.push_back(row_mut);
  }

  CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  std::promise<void> done;
  table.AsyncBulkApply(
      cq,
      [&done](CompletionQueue& cq, std::vector<FailedMutation>& failed,
              grpc::Status const& status) {
        done.set_value();
        EXPECT_EQ(0, failed.size());
        EXPECT_TRUE(status.ok());
      },
      std::move(mut));

  // Block until the asynchronous operation completes. This is not what one
  // would do in a real application (the synchronous API is better in that
  // case), but we need to wait before checking the results.
  done.get_future().get();

  // Validate that the newly created cells are actually in the server.
  std::vector<bigtable::Cell> expected;
  for (auto const& row_cells : created) {
    auto const& cells = row_cells.second;
    for (auto const& c : cells) {
      expected.push_back(c);
    }
  }

  auto actual = ReadRows(sync_table, bigtable::Filter::PassAllFilter());

  // Cleanup the thread running the completion queue event loop.
  cq.Shutdown();
  pool.join();
  CheckEqualUnordered(expected, actual);
}

TEST_F(DataAsyncIntegrationTest, SampleRowKeys) {
  auto sync_table = GetTable();
  auto table = GetNoExTable();

  // Create BATCH_SIZE * BATCH_COUNT rows.
  constexpr int BATCH_COUNT = 10;
  constexpr int BATCH_SIZE = 5000;
  constexpr int COLUMN_COUNT = 10;
  int rowid = 0;
  for (int batch = 0; batch != BATCH_COUNT; ++batch) {
    bigtable::BulkMutation bulk;
    for (int row = 0; row != BATCH_SIZE; ++row) {
      std::ostringstream os;
      os << "row:" << std::setw(9) << std::setfill('0') << rowid;

      // Build a mutation that creates 10 columns.
      bigtable::SingleRowMutation mutation(os.str());
      for (int col = 0; col != COLUMN_COUNT; ++col) {
        std::string colid = "c" + std::to_string(col);
        std::string value = colid + "#" + os.str();
        mutation.emplace_back(
            bigtable::SetCell("family1", std::move(colid), std::move(value)));
      }
      bulk.emplace_back(std::move(mutation));
      ++rowid;
    }
    sync_table.BulkApply(std::move(bulk));
  }
  CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  std::promise<std::vector<RowKeySample>> done;
  table.AsyncSampleRowKeys(
      cq, [&done](CompletionQueue& cq, std::vector<RowKeySample>& samples,
                  grpc::Status const& status) {
        EXPECT_TRUE(status.ok());
        done.set_value(std::move(samples));
      });

  // Block until the asynchronous operation completes. This is not what one
  // would do in a real application (the synchronous API is better in that
  // case), but we need to wait before checking the results.
  auto samples = done.get_future().get();

  cq.Shutdown();
  pool.join();

  // It is somewhat hard to verify that the values returned here are correct.
  // We cannot check the specific values, not even the format, of the row keys
  // because Cloud Bigtable might return an empty row key (for "end of table"),
  // and it might return row keys that have never been written to.
  // All we can check is that this is not empty, and that the offsets are in
  // ascending order.
  EXPECT_FALSE(samples.empty());
  std::int64_t previous = 0;
  for (auto const& s : samples) {
    EXPECT_LE(previous, s.offset_bytes);
    previous = s.offset_bytes;
  }
  // At least one of the samples should have non-zero offset:
  auto last = samples.back();
  EXPECT_LT(0, last.offset_bytes);
}

TEST_F(DataAsyncIntegrationTest, TableCheckAndMutateRowPass) {
  auto sync_table = GetTable();
  auto table = GetNoExTable();

  std::string const key = "row-key";

  std::vector<bigtable::Cell> created{{key, "family1", "c1", 0, "v1000"}};
  CreateCells(sync_table, created);
  std::promise<void> done;
  CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });
  table.AsyncCheckAndMutateRow(
      cq,
      [&done](CompletionQueue& cq, bool response, grpc::Status const& status) {
        done.set_value();
        EXPECT_TRUE(status.ok());
        EXPECT_TRUE(response);
      },
      key, bigtable::Filter::ValueRegex("v1000"),
      {bigtable::SetCell("family1", "c2", 0_ms, "v2000")},
      {bigtable::SetCell("family1", "c3", 0_ms, "v3000")});
  done.get_future().get();
  cq.Shutdown();
  pool.join();
  std::vector<bigtable::Cell> expected{{key, "family1", "c1", 0, "v1000"},
                                       {key, "family1", "c2", 0, "v2000"}};
  auto actual = ReadRows(sync_table, bigtable::Filter::PassAllFilter());
  CheckEqualUnordered(expected, actual);
}

TEST_F(DataAsyncIntegrationTest, TableCheckAndMutateRowFail) {
  auto sync_table = GetTable();
  auto table = GetNoExTable();

  std::string const key = "row-key";

  std::vector<bigtable::Cell> created{{key, "family1", "c1", 0, "v1000"}};
  CreateCells(sync_table, created);
  CompletionQueue cq;
  std::promise<void> done;
  std::thread pool([&cq] { cq.Run(); });
  table.AsyncCheckAndMutateRow(
      cq,
      [&done](CompletionQueue& cq, bool response, grpc::Status const& status) {
        done.set_value();
        EXPECT_TRUE(status.ok());
        EXPECT_FALSE(response);
      },
      key, bigtable::Filter::ValueRegex("not-there"),
      {bigtable::SetCell("family1", "c2", 0_ms, "v2000")},
      {bigtable::SetCell("family1", "c3", 0_ms, "v3000")});
  done.get_future().get();
  cq.Shutdown();
  pool.join();
  std::vector<bigtable::Cell> expected{{key, "family1", "c1", 0, "v1000"},
                                       {key, "family1", "c3", 0, "v3000"}};
  auto actual = ReadRows(sync_table, bigtable::Filter::PassAllFilter());
  CheckEqualUnordered(expected, actual);
}

TEST_F(DataAsyncIntegrationTest, AsyncReadModifyWriteAppendValueTest) {
  auto sync_table = GetTable();
  auto table = GetNoExTable();

  std::string const row_key1 = "row-key-1";
  std::string const row_key2 = "row-key-2";
  std::string const add_suffix1 = "-suffix";
  std::string const add_suffix2 = "-next";
  std::string const add_suffix3 = "-newrecord";

  std::vector<bigtable::Cell> created{
      {row_key1, "family1", "column-id1", 1000, "v1000"},
      {row_key1, "family2", "column-id2", 2000, "v2000"},
      {row_key1, "family3", "column-id1", 2000, "v3000"},
      {row_key1, "family1", "column-id3", 2000, "v5000"}};

  std::vector<bigtable::Cell> expected{
      {row_key1, "family1", "column-id1", 1000, "v1000" + add_suffix1},
      {row_key1, "family2", "column-id2", 2000, "v2000" + add_suffix2},
      {row_key1, "family3", "column-id3", 2000, add_suffix3}};

  CreateCells(sync_table, created);

  CompletionQueue cq;
  google::cloud::promise<Row> done;
  std::thread pool([&cq] { cq.Run(); });

  table.AsyncReadModifyWriteRow(
      cq,
      [&done](CompletionQueue& cq, Row row, grpc::Status& status) {
        done.set_value(std::move(row));
        EXPECT_TRUE(status.ok());
      },
      row_key1,
      bigtable::ReadModifyWriteRule::AppendValue("family1", "column-id1",
                                                 add_suffix1),
      bigtable::ReadModifyWriteRule::AppendValue("family2", "column-id2",
                                                 add_suffix2),
      bigtable::ReadModifyWriteRule::AppendValue("family3", "column-id3",
                                                 add_suffix3));

  auto result_row = done.get_future().get();

  cq.Shutdown();
  pool.join();

  // Returned cells contains timestamp in microseconds which is
  // not matching with the timestamp in expected cells, So creating
  // cells by ignoring timestamp
  auto expected_cells_ignore_timestamp = GetCellsIgnoringTimestamp(expected);
  auto actual_cells_ignore_timestamp =
      GetCellsIgnoringTimestamp(result_row.cells());

  CheckEqualUnordered(expected_cells_ignore_timestamp,
                      actual_cells_ignore_timestamp);
}

TEST_F(DataAsyncIntegrationTest, AsyncReadModifyWriteRowIncrementAmountTest) {
  auto sync_table = GetTable();
  auto table = GetNoExTable();

  std::string const key = "row-key";

  // An initial; big-endian int64 number with value 0.
  std::string v1("\x00\x00\x00\x00\x00\x00\x00\x00", 8);
  std::vector<bigtable::Cell> created{{key, "family1", "c1", 0, v1}};

  // The expected values as buffers containing big-endian int64 numbers.
  std::string e1("\x00\x00\x00\x00\x00\x00\x00\x2A", 8);
  std::string e2("\x00\x00\x00\x00\x00\x00\x00\x07", 8);
  std::vector<bigtable::Cell> expected{{key, "family1", "c1", 0, e1},
                                       {key, "family1", "c2", 0, e2}};

  CreateCells(sync_table, created);

  CompletionQueue cq;
  google::cloud::promise<Row> done;
  std::thread pool([&cq] { cq.Run(); });

  table.AsyncReadModifyWriteRow(
      cq,
      [&done](CompletionQueue& cq, Row row, grpc::Status& status) {
        done.set_value(std::move(row));
        EXPECT_TRUE(status.ok());
      },
      key, bigtable::ReadModifyWriteRule::IncrementAmount("family1", "c1", 42),
      bigtable::ReadModifyWriteRule::IncrementAmount("family1", "c2", 7));
  auto row = done.get_future().get();

  cq.Shutdown();
  pool.join();
  // Ignore the server set timestamp on the returned cells because it is not
  // predictable.
  auto expected_ignore_timestamp = GetCellsIgnoringTimestamp(expected);
  auto actual_ignore_timestamp = GetCellsIgnoringTimestamp(row.cells());

  CheckEqualUnordered(expected_ignore_timestamp, actual_ignore_timestamp);
}

TEST_F(DataAsyncIntegrationTest, AsyncReadModifyWriteRowMultipleTest) {
  auto sync_table = GetTable();
  auto table = GetNoExTable();

  std::string const key = "row-key";

  std::string v1("\x00\x00\x00\x00\x00\x00\x00\x00", 8);
  std::vector<bigtable::Cell> created{{key, "family1", "c1", 0, v1},
                                      {key, "family1", "c3", 0, "start;"},
                                      {key, "family2", "d1", 0, v1},
                                      {key, "family2", "d3", 0, "start;"}};

  // The expected values as buffers containing big-endian int64 numbers.
  std::string e1("\x00\x00\x00\x00\x00\x00\x00\x2A", 8);
  std::string e2("\x00\x00\x00\x00\x00\x00\x00\x07", 8);
  std::string e3("\x00\x00\x00\x00\x00\x00\x07\xD0", 8);
  std::string e4("\x00\x00\x00\x00\x00\x00\x0B\xB8", 8);
  std::vector<bigtable::Cell> expected{
      {key, "family1", "c1", 0, e1},
      {key, "family1", "c2", 0, e2},
      {key, "family1", "c3", 0, "start;suffix"},
      {key, "family1", "c4", 0, "suffix"},
      {key, "family2", "d1", 0, e3},
      {key, "family2", "d2", 0, e4},
      {key, "family2", "d3", 0, "start;suffix"},
      {key, "family2", "d4", 0, "suffix"}};

  CreateCells(sync_table, created);

  CompletionQueue cq;
  google::cloud::promise<Row> done;
  std::thread pool([&cq] { cq.Run(); });
  using R = bigtable::ReadModifyWriteRule;

  table.AsyncReadModifyWriteRow(
      cq,
      [&done](CompletionQueue& cq, Row row, grpc::Status& status) {
        done.set_value(std::move(row));
        EXPECT_TRUE(status.ok());
      },
      key, R::IncrementAmount("family1", "c1", 42),
      R::IncrementAmount("family1", "c2", 7),
      R::IncrementAmount("family2", "d1", 2000),
      R::IncrementAmount("family2", "d2", 3000),
      R::AppendValue("family1", "c3", "suffix"),
      R::AppendValue("family1", "c4", "suffix"),
      R::AppendValue("family2", "d3", "suffix"),
      R::AppendValue("family2", "d4", "suffix"));

  auto row = done.get_future().get();

  cq.Shutdown();
  pool.join();

  // Ignore the server set timestamp on the returned cells because it is not
  // predictable.
  auto expected_ignore_timestamp = GetCellsIgnoringTimestamp(expected);
  auto actual_ignore_timestamp = GetCellsIgnoringTimestamp(row.cells());

  CheckEqualUnordered(expected_ignore_timestamp, actual_ignore_timestamp);
}

TEST_F(DataAsyncIntegrationTest, TableReadRowsAllRows) {
  auto sync_table = GetTable();
  auto table = GetNoExTable();

  std::string const row_key1 = "row-key-1";
  std::string const row_key2 = "row-key-2";
  std::string const row_key3(1024, '3');    // a long key
  std::string const long_value(1024, 'v');  // a long value

  std::vector<bigtable::Cell> created{
      {row_key1, "family1", "c1", 1000, "data1"},
      {row_key1, "family1", "c2", 1000, "data2"},
      {row_key2, "family1", "c1", 1000, ""},
      {row_key3, "family1", "c1", 1000, long_value}};

  CreateCells(sync_table, created);

  CompletionQueue cq;
  std::promise<void> done;
  std::thread pool([&cq] { cq.Run(); });

  std::vector<bigtable::Cell> actual;

  table.AsyncReadRows(
      cq,
      [&actual](CompletionQueue& cq, Row row, grpc::Status& status) {
        std::move(row.cells().begin(), row.cells().end(),
                  std::back_inserter(actual));
      },
      [&done](CompletionQueue& cq, bool& response, grpc::Status const& status) {
        done.set_value();
        EXPECT_TRUE(status.ok());
      },
      bigtable::RowSet(bigtable::RowRange::InfiniteRange()),
      RowReader::NO_ROWS_LIMIT, Filter::PassAllFilter());

  done.get_future().get();

  cq.Shutdown();
  pool.join();

  CheckEqualUnordered(created, actual);
}

TEST_F(DataAsyncIntegrationTest, TableAsyncReadRow) {
  auto sync_table = GetTable();
  auto table = GetNoExTable();

  std::string const row_key1 = "row-key-1";
  std::string const row_key2 = "row-key-2";

  std::vector<bigtable::Cell> created{
      {row_key1, "family1", "c1", 1000, "v1000"},
      {row_key2, "family1", "c2", 2000, "v2000"}};
  std::vector<bigtable::Cell> expected{
      {row_key1, "family1", "c1", 1000, "v1000"}};

  CreateCells(sync_table, created);

  CompletionQueue cq;
  google::cloud::promise<std::pair<bool, Row>> done;
  std::thread pool([&cq] { cq.Run(); });

  table.AsyncReadRow(cq,
                     [&done](CompletionQueue& cq, std::pair<bool, Row> response,
                             grpc::Status const& status) {
                       done.set_value(response);
                       EXPECT_TRUE(status.ok());
                     },
                     "row-key-1", Filter::PassAllFilter());

  auto response = done.get_future().get();
  std::vector<bigtable::Cell> actual;
  actual.emplace_back(response.second.cells().at(0));

  cq.Shutdown();
  pool.join();

  CheckEqualUnordered(expected, actual);
  EXPECT_TRUE(response.first);
}

TEST_F(DataAsyncIntegrationTest, TableAsyncReadRowForNoRow) {
  auto sync_table = GetTable();
  auto table = GetNoExTable();

  std::string const row_key2 = "row-key-2";

  std::vector<bigtable::Cell> created{
      {row_key2, "family1", "c2", 2000, "v2000"}};

  CreateCells(sync_table, created);

  CompletionQueue cq;
  google::cloud::promise<std::pair<bool, Row>> done;
  std::thread pool([&cq] { cq.Run(); });

  table.AsyncReadRow(cq,
                     [&done](CompletionQueue& cq, std::pair<bool, Row> response,
                             grpc::Status const& status) {
                       done.set_value(response);
                       EXPECT_TRUE(status.ok());
                     },
                     "row-key-1", Filter::PassAllFilter());

  auto response = done.get_future().get();

  cq.Shutdown();
  pool.join();

  EXPECT_FALSE(response.first);
  EXPECT_EQ(0, response.second.cells().size());
}
}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Make sure the arguments are valid.
  if (argc != 3) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project> <instance>\n";
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];

  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::bigtable::testing::TableTestEnvironment(project_id,
                                                                 instance_id));

  return RUN_ALL_TESTS();
}
