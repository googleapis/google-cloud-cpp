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

#include "bigtable/client/readrowsparser.h"

#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <numeric>
#include <sstream>
#include <vector>

using google::bigtable::v2::ReadRowsResponse_CellChunk;

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
  *os << "rk: " << std::string(c.row_key()) << "\n";
  *os << "fm: " << std::string(c.family_name()) << "\n";
  *os << "qual: " << std::string(c.column_qualifier()) << "\n";
  *os << "ts: " << c.timestamp() << "\n";
  *os << "value: " << std::string(c.value()) << "\n";
  *os << "label: " << LabelsToString(c.labels()) << "\n";
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
      std::transform(r.cells().begin(), r.cells().end(),
                     std::back_inserter(cells), bigtable::CellToString);
    }
    return cells;
  }

  std::vector<ReadRowsResponse_CellChunk> ConvertChunks(
      std::vector<std::string> chunk_strings) {
    using google::protobuf::TextFormat;

    std::vector<ReadRowsResponse_CellChunk> chunks;
    for (const std::string& chunk_string : chunk_strings) {
      ReadRowsResponse_CellChunk chunk;
      if (not TextFormat::ParseFromString(chunk_string, &chunk)) {
        return {};
      }
      chunks.emplace_back(std::move(chunk));
    }

    return chunks;
  }

  void FeedChunks(std::vector<ReadRowsResponse_CellChunk> chunks) {
    for (const auto& chunk : chunks) {
      parser_.HandleChunk(chunk);
      if (parser_.HasNext()) {
        rows_.emplace_back(parser_.Next());
      }
    }
    parser_.HandleEOT();
  }

 private:
  bigtable::ReadRowsParser parser_;
  std::vector<bigtable::Row> rows_;
};

// Test name: "bare commit implies ts=0"
TEST_F(AcceptanceTest, BareCommitImpliesTs) {
  std::vector<std::string> chunk_strings = {
      R"chunk(
          row_key: "RK"
          family_name: < value: "A">
          qualifier: < value: "C">
          timestamp_micros: 100
          value: "value-VAL"
          commit_row: false
        )chunk",
      R"chunk(
          commit_row: true
        )chunk",
  };

  auto chunks = ConvertChunks(std::move(chunk_strings));
  ASSERT_FALSE(chunks.empty());
  EXPECT_NO_THROW(FeedChunks(chunks));

  std::vector<std::string> expected_cells = {
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
  };
  EXPECT_EQ(expected_cells, ExtractCells());
}

// Test name: "invalid - bare reset"
TEST_F(AcceptanceTest, InvalidBareReset) {
  std::vector<std::string> chunk_strings = {
      R"chunk(
          reset_row: true
        )chunk",
  };

  auto chunks = ConvertChunks(std::move(chunk_strings));
  ASSERT_FALSE(chunks.empty());
  EXPECT_THROW(FeedChunks(chunks), std::exception);

  std::vector<std::string> expected_cells = {};
  EXPECT_EQ(expected_cells, ExtractCells());
}

// Test name: "invalid - last row missing commit"
TEST_F(AcceptanceTest, InvalidLastRowMissingCommit) {
  std::vector<std::string> chunk_strings = {
      R"chunk(
          row_key: "RK_1"
          family_name: < value: "A">
          qualifier: < value: "C">
          timestamp_micros: 100
          value: "value-VAL"
          commit_row: true
        )chunk",
      R"chunk(
          row_key: "RK_2"
          family_name: < value: "A">
          qualifier: < value: "C">
          timestamp_micros: 100
          value: "value-VAL"
          commit_row: false
        )chunk",
  };

  auto chunks = ConvertChunks(std::move(chunk_strings));
  ASSERT_FALSE(chunks.empty());
  EXPECT_THROW(FeedChunks(chunks), std::exception);

  std::vector<std::string> expected_cells = {
      "rk: RK_1\n"
      "fm: A\n"
      "qual: C\n"
      "ts: 100\n"
      "value: value-VAL\n"
      "label: \n",
  };
  EXPECT_EQ(expected_cells, ExtractCells());
}
