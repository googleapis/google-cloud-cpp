// Copyright 2017 Google Inc.
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

#include "bigtable/client/data.h"

#include <array>
#include <iostream>
#include <numeric>
#include <sstream>

#include <absl/memory/memory.h>
#include <gmock/gmock.h>
#include <google/bigtable/v2/bigtable_mock.grpc.pb.h>
#include <google/protobuf/text_format.h>

#include <bigtable/client/readrowsparser.h>

using google::bigtable::v2::ReadRowsResponse;

// TODO(dmahu): this is copied from "grpc++/test/mock_stream.h"
// which is not installed by default by grpc.
template <class R>
class MockClientReader : public grpc::ClientReaderInterface<R> {
 public:
  MockClientReader() = default;

  /// ClientStreamingInterface
  MOCK_METHOD0_T(Finish, grpc::Status());

  /// ReaderInterface
  MOCK_METHOD1_T(NextMessageSize, bool(uint32_t*));
  MOCK_METHOD1_T(Read, bool(R*));

  /// ClientReaderInterface
  MOCK_METHOD0_T(WaitForInitialMetadata, void());
};

class TableForTesting : public bigtable::Table {
 public:
  TableForTesting() : bigtable::Table(nullptr, "") {}

  // Acts as if the responses in the fixture were received over the
  // wire, and fills rows with the callback arguments. If stop_after
  // is non-zero, returns false from the callback after stop_after
  // invocations.
  grpc::Status ReadRowsFromFixture(const std::vector<ReadRowsResponse>& fixture,
                                   std::vector<bigtable::RowPart>* rows,
                                   int stop_after) {
    auto stream = std::unique_ptr<MockClientReader<ReadRowsResponse>>(
        new MockClientReader<ReadRowsResponse>);
    grpc::Status stream_status;

    int read_count = 0;
    auto fake_read = [&read_count, &fixture](ReadRowsResponse* response) {
      if (read_count < fixture.size()) {
        *response = fixture[read_count];
      }
      return read_count++ < fixture.size();
    };

    EXPECT_CALL(*stream, Finish())
        .WillOnce(testing::ReturnPointee(&stream_status));
    EXPECT_CALL(*stream, Read(testing::_))
        .Times(fixture.size() + 1)
        .WillRepeatedly(testing::Invoke(fake_read));

    bigtable::Table::ReadStream read_stream(
        absl::make_unique<grpc::ClientContext>(), std::move(stream),
        absl::make_unique<bigtable::ReadRowsParser>());

    for (const auto& row : read_stream) {
      rows->push_back(row);
      if (rows->size() == stop_after) {
        read_stream.Cancel();
        stream_status = grpc::Status(grpc::CANCELLED, "");
      }
    }

    return read_stream.FinalStatus();
  }
};

TEST(DataTest, ReadRowsEmptyReply) {
  TableForTesting table;
  std::vector<ReadRowsResponse> fixture;
  std::vector<bigtable::RowPart> rows;

  grpc::Status s = table.ReadRowsFromFixture(fixture, &rows, 0);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(rows.size(), 0);
}

TEST(DataTest, ReadRowsSingleRowSingleChunk) {
  TableForTesting table;
  std::vector<ReadRowsResponse> fixture;
  std::vector<bigtable::RowPart> rows;

  ReadRowsResponse rrr;
  auto chunk = rrr.add_chunks();
  chunk->set_row_key("r1");
  chunk->mutable_family_name()->set_value("fam");
  chunk->mutable_qualifier()->set_value("qual");
  chunk->set_timestamp_micros(42000);
  chunk->set_commit_row(true);

  fixture.push_back(rrr);

  grpc::Status s = table.ReadRowsFromFixture(fixture, &rows, 0);

  EXPECT_TRUE(s.ok());
  ASSERT_EQ(rows.size(), 1);
  EXPECT_EQ(rows[0].row(), "r1");
}

TEST(DataTest, ReadRowsReplyWithNoChunk) {
  TableForTesting table;
  std::vector<ReadRowsResponse> fixture;
  std::vector<bigtable::RowPart> rows;

  ReadRowsResponse rrr;

  fixture.push_back(rrr);

  grpc::Status s = table.ReadRowsFromFixture(fixture, &rows, 0);

  EXPECT_TRUE(s.ok());
  ASSERT_EQ(rows.size(), 0);
}

// **** Helper declarations for acceptance tests

void AddChunk(std::vector<ReadRowsResponse>* fixture, std::string chunk_repr) {
  ReadRowsResponse rrr;
  auto* chunk = rrr.add_chunks();
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(chunk_repr, chunk));

  fixture->push_back(rrr);
}

// syntactic sugar
std::vector<ReadRowsResponse>& operator<<(
    std::vector<ReadRowsResponse>& fixture, const std::string& chunk_repr) {
  AddChunk(&fixture, chunk_repr);
  return fixture;
}

namespace bigtable {
namespace {

std::string LabelsToString(const std::vector<std::string>& labels) {
  if (labels.empty()) return "";
  return std::accumulate(
      std::next(labels.begin()), labels.end(), labels[0],
      [](std::string a, std::string b) { return a + ',' + b; });
}

// Can also be used by gtest to print Cell values
void PrintTo(const Cell& c, ::std::ostream* os) {
  *os << "rk: " << c.row << "\n";
  *os << "fm: " << c.family << "\n";
  *os << "qual: " << c.column << "\n";
  *os << "ts: " << c.timestamp << "\n";
  *os << "value: " << c.value << "\n";
  *os << "label: " << LabelsToString(c.labels) << "\n";
}

std::string CellToString(const Cell& cell) {
  std::stringstream ss;
  PrintTo(cell, &ss);
  return ss.str();
}

};  // namespace
};  // namespace bigtable

class AcceptanceTest : public ::testing::Test {
 protected:
  std::vector<std::string> ExtractCells() {
    std::vector<std::string> cells;

    for (const auto& r : rows_) {
      std::transform(r.begin(), r.end(), std::back_inserter(cells),
                     bigtable::CellToString);
    }
    return cells;
  }

  void ReadAndExpectResult(bool is_expected_ok) {
    grpc::Status read_rows_status =
        table_.ReadRowsFromFixture(fixture_, &rows_, 0);

    EXPECT_EQ(read_rows_status.ok(), is_expected_ok);
  }

  std::vector<ReadRowsResponse>& fixture() { return fixture_; }

 private:
  TableForTesting table_;
  std::vector<ReadRowsResponse> fixture_;
  std::vector<bigtable::RowPart> rows_;
};

// The tests included between the markers below are defined in the
// file "read-rows-acceptance-test.json" in the cloud-bigtable-client
// repository and formatted as C++ code using tools/convert_tests.py

// **** AUTOMATICALLY GENERATED ACCEPTANCE TESTS BEGIN HERE ****

// Test name: "invalid - no commit"
TEST_F(AcceptanceTest, InvalidNoCommit) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: false";

  ReadAndExpectResult(false);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({}));
}

// Test name: "invalid - no cell key before commit"
TEST_F(AcceptanceTest, InvalidNoCellKeyBeforeCommit) {
  fixture() << "commit_row: true";

  ReadAndExpectResult(false);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({}));
}

// Test name: "invalid - no cell key before value"
TEST_F(AcceptanceTest, InvalidNoCellKeyBeforeValue) {
  fixture() << "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: false";

  ReadAndExpectResult(false);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({}));
}

// Test name: "invalid - new col family must specify qualifier"
TEST_F(AcceptanceTest, InvalidNewColFamilyMustSpecifyQualifier) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 99\n"
               "value: \"value-VAL_1\"\n"
               "commit_row: false"
            << "family_name: < value: \"B\">\n"
               "timestamp_micros: 98\n"
               "value: \"value-VAL_2\"\n"
               "commit_row: true";

  ReadAndExpectResult(false);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({}));
}

// Test name: "bare commit implies ts=0"
TEST_F(AcceptanceTest, BareCommitImpliesTs) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: false"
            << "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL\n"
                                "label: \n",

                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 0\n"
                                "value: \n"
                                "label: \n",
                            }));
}

// Test name: "simple row with timestamp"
TEST_F(AcceptanceTest, SimpleRowWithTimestamp) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL\n"
                                "label: \n",
                            }));
}

// Test name: "missing timestamp, implied ts=0"
TEST_F(AcceptanceTest, MissingTimestampImpliedTs) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "value: \"value-VAL\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 0\n"
                                "value: value-VAL\n"
                                "label: \n",
                            }));
}

// Test name: "empty cell value"
TEST_F(AcceptanceTest, EmptyCellValue) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 0\n"
                                "value: \n"
                                "label: \n",
                            }));
}

// Test name: "two unsplit cells"
TEST_F(AcceptanceTest, TwoUnsplitCells) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 99\n"
               "value: \"value-VAL_1\"\n"
               "commit_row: false"
            << "timestamp_micros: 98\n"
               "value: \"value-VAL_2\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 99\n"
                                "value: value-VAL_1\n"
                                "label: \n",

                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 98\n"
                                "value: value-VAL_2\n"
                                "label: \n",
                            }));
}

// Test name: "two qualifiers"
TEST_F(AcceptanceTest, TwoQualifiers) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 99\n"
               "value: \"value-VAL_1\"\n"
               "commit_row: false"
            << "qualifier: < value: \"D\">\n"
               "timestamp_micros: 98\n"
               "value: \"value-VAL_2\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 99\n"
                                "value: value-VAL_1\n"
                                "label: \n",

                                "rk: RK\n"
                                "fm: A\n"
                                "qual: D\n"
                                "ts: 98\n"
                                "value: value-VAL_2\n"
                                "label: \n",
                            }));
}

// Test name: "two families"
TEST_F(AcceptanceTest, TwoFamilies) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 99\n"
               "value: \"value-VAL_1\"\n"
               "commit_row: false"
            << "family_name: < value: \"B\">\n"
               "qualifier: < value: \"E\">\n"
               "timestamp_micros: 98\n"
               "value: \"value-VAL_2\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 99\n"
                                "value: value-VAL_1\n"
                                "label: \n",

                                "rk: RK\n"
                                "fm: B\n"
                                "qual: E\n"
                                "ts: 98\n"
                                "value: value-VAL_2\n"
                                "label: \n",
                            }));
}

// Test name: "with labels"
TEST_F(AcceptanceTest, WithLabels) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 99\n"
               "labels: \"L_1\"\n"
               "value: \"value-VAL_1\"\n"
               "commit_row: false"
            << "timestamp_micros: 98\n"
               "labels: \"L_2\"\n"
               "value: \"value-VAL_2\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 99\n"
                                "value: value-VAL_1\n"
                                "label: L_1\n",

                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 98\n"
                                "value: value-VAL_2\n"
                                "label: L_2\n",
                            }));
}

// Test name: "split cell, bare commit"
TEST_F(AcceptanceTest, SplitCellBareCommit) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"v\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"alue-VAL\"\n"
               "commit_row: false"
            << "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL\n"
                                "label: \n",

                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 0\n"
                                "value: \n"
                                "label: \n",
                            }));
}

// Test name: "split cell"
TEST_F(AcceptanceTest, SplitCell) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"v\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"alue-VAL\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL\n"
                                "label: \n",
                            }));
}

// Test name: "split four ways"
TEST_F(AcceptanceTest, SplitFourWays) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "labels: \"L\"\n"
               "value: \"v\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"a\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"l\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"ue-VAL\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL\n"
                                "label: L\n",
                            }));
}

// Test name: "two split cells"
TEST_F(AcceptanceTest, TwoSplitCells) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 99\n"
               "value: \"v\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"alue-VAL_1\"\n"
               "commit_row: false"
            << "timestamp_micros: 98\n"
               "value: \"v\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"alue-VAL_2\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 99\n"
                                "value: value-VAL_1\n"
                                "label: \n",

                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 98\n"
                                "value: value-VAL_2\n"
                                "label: \n",
                            }));
}

// Test name: "multi-qualifier splits"
TEST_F(AcceptanceTest, MultiqualifierSplits) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 99\n"
               "value: \"v\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"alue-VAL_1\"\n"
               "commit_row: false"
            << "qualifier: < value: \"D\">\n"
               "timestamp_micros: 98\n"
               "value: \"v\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"alue-VAL_2\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 99\n"
                                "value: value-VAL_1\n"
                                "label: \n",

                                "rk: RK\n"
                                "fm: A\n"
                                "qual: D\n"
                                "ts: 98\n"
                                "value: value-VAL_2\n"
                                "label: \n",
                            }));
}

// Test name: "multi-qualifier multi-split"
TEST_F(AcceptanceTest, MultiqualifierMultisplit) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 99\n"
               "value: \"v\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"a\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"lue-VAL_1\"\n"
               "commit_row: false"
            << "qualifier: < value: \"D\">\n"
               "timestamp_micros: 98\n"
               "value: \"v\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"a\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"lue-VAL_2\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 99\n"
                                "value: value-VAL_1\n"
                                "label: \n",

                                "rk: RK\n"
                                "fm: A\n"
                                "qual: D\n"
                                "ts: 98\n"
                                "value: value-VAL_2\n"
                                "label: \n",
                            }));
}

// Test name: "multi-family split"
TEST_F(AcceptanceTest, MultifamilySplit) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 99\n"
               "value: \"v\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"alue-VAL_1\"\n"
               "commit_row: false"
            << "family_name: < value: \"B\">\n"
               "qualifier: < value: \"E\">\n"
               "timestamp_micros: 98\n"
               "value: \"v\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"alue-VAL_2\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 99\n"
                                "value: value-VAL_1\n"
                                "label: \n",

                                "rk: RK\n"
                                "fm: B\n"
                                "qual: E\n"
                                "ts: 98\n"
                                "value: value-VAL_2\n"
                                "label: \n",
                            }));
}

// Test name: "invalid - no commit between rows"
TEST_F(AcceptanceTest, InvalidNoCommitBetweenRows) {
  fixture() << "row_key: \"RK_1\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: false"
            << "row_key: \"RK_2\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: false";

  ReadAndExpectResult(false);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({}));
}

// Test name: "invalid - no commit after first row"
TEST_F(AcceptanceTest, InvalidNoCommitAfterFirstRow) {
  fixture() << "row_key: \"RK_1\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: false"
            << "row_key: \"RK_2\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: true";

  ReadAndExpectResult(false);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({}));
}

// Test name: "invalid - last row missing commit"
TEST_F(AcceptanceTest, InvalidLastRowMissingCommit) {
  fixture() << "row_key: \"RK_1\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: true"
            << "row_key: \"RK_2\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: false";

  ReadAndExpectResult(false);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK_1\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL\n"
                                "label: \n",
                            }));
}

// Test name: "invalid - duplicate row key"
TEST_F(AcceptanceTest, InvalidDuplicateRowKey) {
  fixture() << "row_key: \"RK_1\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: true"
            << "row_key: \"RK_1\"\n"
               "family_name: < value: \"B\">\n"
               "qualifier: < value: \"D\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: true";

  ReadAndExpectResult(false);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK_1\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL\n"
                                "label: \n",
                            }));
}

// Test name: "invalid - new row missing row key"
TEST_F(AcceptanceTest, InvalidNewRowMissingRowKey) {
  fixture() << "row_key: \"RK_1\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: true"
            << "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: true";

  ReadAndExpectResult(false);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK_1\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL\n"
                                "label: \n",
                            }));
}

// Test name: "two rows"
TEST_F(AcceptanceTest, TwoRows) {
  fixture() << "row_key: \"RK_1\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: true"
            << "row_key: \"RK_2\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK_1\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL\n"
                                "label: \n",

                                "rk: RK_2\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL\n"
                                "label: \n",
                            }));
}

// Test name: "two rows implicit timestamp"
TEST_F(AcceptanceTest, TwoRowsImplicitTimestamp) {
  fixture() << "row_key: \"RK_1\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "value: \"value-VAL\"\n"
               "commit_row: true"
            << "row_key: \"RK_2\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK_1\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 0\n"
                                "value: value-VAL\n"
                                "label: \n",

                                "rk: RK_2\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL\n"
                                "label: \n",
                            }));
}

// Test name: "two rows empty value"
TEST_F(AcceptanceTest, TwoRowsEmptyValue) {
  fixture() << "row_key: \"RK_1\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "commit_row: true"
            << "row_key: \"RK_2\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK_1\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 0\n"
                                "value: \n"
                                "label: \n",

                                "rk: RK_2\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL\n"
                                "label: \n",
                            }));
}

// Test name: "two rows, one with multiple cells"
TEST_F(AcceptanceTest, TwoRowsOneWithMultipleCells) {
  fixture() << "row_key: \"RK_1\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 99\n"
               "value: \"value-VAL_1\"\n"
               "commit_row: false"
            << "timestamp_micros: 98\n"
               "value: \"value-VAL_2\"\n"
               "commit_row: true"
            << "row_key: \"RK_2\"\n"
               "family_name: < value: \"B\">\n"
               "qualifier: < value: \"D\">\n"
               "timestamp_micros: 97\n"
               "value: \"value-VAL_3\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK_1\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 99\n"
                                "value: value-VAL_1\n"
                                "label: \n",

                                "rk: RK_1\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 98\n"
                                "value: value-VAL_2\n"
                                "label: \n",

                                "rk: RK_2\n"
                                "fm: B\n"
                                "qual: D\n"
                                "ts: 97\n"
                                "value: value-VAL_3\n"
                                "label: \n",
                            }));
}

// Test name: "two rows, multiple cells"
TEST_F(AcceptanceTest, TwoRowsMultipleCells) {
  fixture() << "row_key: \"RK_1\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 99\n"
               "value: \"value-VAL_1\"\n"
               "commit_row: false"
            << "qualifier: < value: \"D\">\n"
               "timestamp_micros: 98\n"
               "value: \"value-VAL_2\"\n"
               "commit_row: true"
            << "row_key: \"RK_2\"\n"
               "family_name: < value: \"B\">\n"
               "qualifier: < value: \"E\">\n"
               "timestamp_micros: 97\n"
               "value: \"value-VAL_3\"\n"
               "commit_row: false"
            << "qualifier: < value: \"F\">\n"
               "timestamp_micros: 96\n"
               "value: \"value-VAL_4\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK_1\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 99\n"
                                "value: value-VAL_1\n"
                                "label: \n",

                                "rk: RK_1\n"
                                "fm: A\n"
                                "qual: D\n"
                                "ts: 98\n"
                                "value: value-VAL_2\n"
                                "label: \n",

                                "rk: RK_2\n"
                                "fm: B\n"
                                "qual: E\n"
                                "ts: 97\n"
                                "value: value-VAL_3\n"
                                "label: \n",

                                "rk: RK_2\n"
                                "fm: B\n"
                                "qual: F\n"
                                "ts: 96\n"
                                "value: value-VAL_4\n"
                                "label: \n",
                            }));
}

// Test name: "two rows, multiple cells, multiple families"
TEST_F(AcceptanceTest, TwoRowsMultipleCellsMultipleFamilies) {
  fixture() << "row_key: \"RK_1\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 99\n"
               "value: \"value-VAL_1\"\n"
               "commit_row: false"
            << "family_name: < value: \"B\">\n"
               "qualifier: < value: \"E\">\n"
               "timestamp_micros: 98\n"
               "value: \"value-VAL_2\"\n"
               "commit_row: true"
            << "row_key: \"RK_2\"\n"
               "family_name: < value: \"M\">\n"
               "qualifier: < value: \"O\">\n"
               "timestamp_micros: 97\n"
               "value: \"value-VAL_3\"\n"
               "commit_row: false"
            << "family_name: < value: \"N\">\n"
               "qualifier: < value: \"P\">\n"
               "timestamp_micros: 96\n"
               "value: \"value-VAL_4\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK_1\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 99\n"
                                "value: value-VAL_1\n"
                                "label: \n",

                                "rk: RK_1\n"
                                "fm: B\n"
                                "qual: E\n"
                                "ts: 98\n"
                                "value: value-VAL_2\n"
                                "label: \n",

                                "rk: RK_2\n"
                                "fm: M\n"
                                "qual: O\n"
                                "ts: 97\n"
                                "value: value-VAL_3\n"
                                "label: \n",

                                "rk: RK_2\n"
                                "fm: N\n"
                                "qual: P\n"
                                "ts: 96\n"
                                "value: value-VAL_4\n"
                                "label: \n",
                            }));
}

// Test name: "two rows, four cells, 2 labels"
TEST_F(AcceptanceTest, TwoRowsFourCellsLabels) {
  fixture() << "row_key: \"RK_1\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 99\n"
               "labels: \"L_1\"\n"
               "value: \"value-VAL_1\"\n"
               "commit_row: false"
            << "timestamp_micros: 98\n"
               "value: \"value-VAL_2\"\n"
               "commit_row: true"
            << "row_key: \"RK_2\"\n"
               "family_name: < value: \"B\">\n"
               "qualifier: < value: \"D\">\n"
               "timestamp_micros: 97\n"
               "labels: \"L_3\"\n"
               "value: \"value-VAL_3\"\n"
               "commit_row: false"
            << "timestamp_micros: 96\n"
               "value: \"value-VAL_4\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK_1\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 99\n"
                                "value: value-VAL_1\n"
                                "label: L_1\n",

                                "rk: RK_1\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 98\n"
                                "value: value-VAL_2\n"
                                "label: \n",

                                "rk: RK_2\n"
                                "fm: B\n"
                                "qual: D\n"
                                "ts: 97\n"
                                "value: value-VAL_3\n"
                                "label: L_3\n",

                                "rk: RK_2\n"
                                "fm: B\n"
                                "qual: D\n"
                                "ts: 96\n"
                                "value: value-VAL_4\n"
                                "label: \n",
                            }));
}

// Test name: "two rows with splits, same timestamp"
TEST_F(AcceptanceTest, TwoRowsWithSplitsSameTimestamp) {
  fixture() << "row_key: \"RK_1\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"v\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"alue-VAL_1\"\n"
               "commit_row: true"
            << "row_key: \"RK_2\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"v\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"alue-VAL_2\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK_1\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL_1\n"
                                "label: \n",

                                "rk: RK_2\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL_2\n"
                                "label: \n",
                            }));
}

// Test name: "invalid - bare reset"
TEST_F(AcceptanceTest, InvalidBareReset) {
  fixture() << "reset_row: true";

  ReadAndExpectResult(false);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({}));
}

// Test name: "invalid - bad reset, no commit"
TEST_F(AcceptanceTest, InvalidBadResetNoCommit) {
  fixture() << "reset_row: true"
            << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: false";

  ReadAndExpectResult(false);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({}));
}

// Test name: "invalid - missing key after reset"
TEST_F(AcceptanceTest, InvalidMissingKeyAfterReset) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: false"
            << "reset_row: true"
            << "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: true";

  ReadAndExpectResult(false);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({}));
}

// Test name: "no data after reset"
TEST_F(AcceptanceTest, NoDataAfterReset) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: false"
            << "reset_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({}));
}

// Test name: "simple reset"
TEST_F(AcceptanceTest, SimpleReset) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: false"
            << "reset_row: true"
            << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL\n"
                                "label: \n",
                            }));
}

// Test name: "reset to new val"
TEST_F(AcceptanceTest, ResetToNewVal) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL_1\"\n"
               "commit_row: false"
            << "reset_row: true"
            << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL_2\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL_2\n"
                                "label: \n",
                            }));
}

// Test name: "reset to new qual"
TEST_F(AcceptanceTest, ResetToNewQual) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL_1\"\n"
               "commit_row: false"
            << "reset_row: true"
            << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"D\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL_1\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: D\n"
                                "ts: 100\n"
                                "value: value-VAL_1\n"
                                "label: \n",
                            }));
}

// Test name: "reset with splits"
TEST_F(AcceptanceTest, ResetWithSplits) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL_1\"\n"
               "commit_row: false"
            << "timestamp_micros: 98\n"
               "value: \"value-VAL_2\"\n"
               "commit_row: false"
            << "reset_row: true"
            << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL_2\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL_2\n"
                                "label: \n",
                            }));
}

// Test name: "reset two cells"
TEST_F(AcceptanceTest, ResetTwoCells) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL_1\"\n"
               "commit_row: false"
            << "reset_row: true"
            << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL_2\"\n"
               "commit_row: false"
            << "timestamp_micros: 97\n"
               "value: \"value-VAL_3\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL_2\n"
                                "label: \n",

                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 97\n"
                                "value: value-VAL_3\n"
                                "label: \n",
                            }));
}

// Test name: "two resets"
TEST_F(AcceptanceTest, TwoResets) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL_1\"\n"
               "commit_row: false"
            << "reset_row: true"
            << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL_2\"\n"
               "commit_row: false"
            << "reset_row: true"
            << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL_3\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL_3\n"
                                "label: \n",
                            }));
}

// Test name: "reset then two cells"
TEST_F(AcceptanceTest, ResetThenTwoCells) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL_1\"\n"
               "commit_row: false"
            << "reset_row: true"
            << "row_key: \"RK\"\n"
               "family_name: < value: \"B\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL_2\"\n"
               "commit_row: false"
            << "qualifier: < value: \"D\">\n"
               "timestamp_micros: 97\n"
               "value: \"value-VAL_3\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: B\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL_2\n"
                                "label: \n",

                                "rk: RK\n"
                                "fm: B\n"
                                "qual: D\n"
                                "ts: 97\n"
                                "value: value-VAL_3\n"
                                "label: \n",
                            }));
}

// Test name: "reset to new row"
TEST_F(AcceptanceTest, ResetToNewRow) {
  fixture() << "row_key: \"RK_1\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL_1\"\n"
               "commit_row: false"
            << "reset_row: true"
            << "row_key: \"RK_2\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL_2\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK_2\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL_2\n"
                                "label: \n",
                            }));
}

// Test name: "reset in between chunks"
TEST_F(AcceptanceTest, ResetInBetweenChunks) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "labels: \"L\"\n"
               "value: \"v\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"a\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "reset_row: true"
            << "row_key: \"RK_1\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL_1\"\n"
               "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK_1\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL_1\n"
                                "label: \n",
                            }));
}

// Test name: "invalid - reset with chunk"
TEST_F(AcceptanceTest, InvalidResetWithChunk) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "labels: \"L\"\n"
               "value: \"v\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"a\"\n"
               "value_size: 10\n"
               "reset_row: true";

  ReadAndExpectResult(false);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({}));
}

// Test name: "invalid - commit with chunk"
TEST_F(AcceptanceTest, InvalidCommitWithChunk) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "labels: \"L\"\n"
               "value: \"v\"\n"
               "value_size: 10\n"
               "commit_row: false"
            << "value: \"a\"\n"
               "value_size: 10\n"
               "commit_row: true";

  ReadAndExpectResult(false);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({}));
}

// Test name: "empty cell chunk"
TEST_F(AcceptanceTest, EmptyCellChunk) {
  fixture() << "row_key: \"RK\"\n"
               "family_name: < value: \"A\">\n"
               "qualifier: < value: \"C\">\n"
               "timestamp_micros: 100\n"
               "value: \"value-VAL\"\n"
               "commit_row: false"
            << "commit_row: false"
            << "commit_row: true";

  ReadAndExpectResult(true);

  EXPECT_EQ(ExtractCells(), std::vector<std::string>({
                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 100\n"
                                "value: value-VAL\n"
                                "label: \n",

                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 0\n"
                                "value: \n"
                                "label: \n",

                                "rk: RK\n"
                                "fm: A\n"
                                "qual: C\n"
                                "ts: 0\n"
                                "value: \n"
                                "label: \n",
                            }));
}

// **** AUTOMATICALLY GENERATED ACCEPTANCE TESTS END HERE ****
