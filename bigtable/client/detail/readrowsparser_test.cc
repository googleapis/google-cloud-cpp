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

#include "bigtable/client/detail/readrowsparser.h"

#include <absl/strings/str_join.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <numeric>
#include <sstream>
#include <vector>

using google::bigtable::v2::ReadRowsResponse_CellChunk;

namespace bigtable {
namespace {

// Can also be used by gtest to print Cell values
void PrintTo(Cell const& c, std::ostream* os) {
  *os << "rk: " << std::string(c.row_key()) << "\n";
  *os << "fm: " << std::string(c.family_name()) << "\n";
  *os << "qual: " << std::string(c.column_qualifier()) << "\n";
  *os << "ts: " << c.timestamp() << "\n";
  *os << "value: " << std::string(c.value()) << "\n";
  *os << "label: " << absl::StrJoin(c.labels(), ",") << "\n";
}

std::string CellToString(Cell const& cell) {
  std::stringstream ss;
  PrintTo(cell, &ss);
  return ss.str();
}

}  // namespace
}  // namespace bigtable

class AcceptanceTest : public ::testing::Test {
 protected:
  std::vector<std::string> ExtractCells() {
    std::vector<std::string> cells;

    for (auto const& r : rows_) {
      std::transform(r.cells().begin(), r.cells().end(),
                     std::back_inserter(cells), bigtable::CellToString);
    }
    return cells;
  }

  std::vector<ReadRowsResponse_CellChunk> ConvertChunks(
      std::vector<std::string> chunk_strings) {
    using google::protobuf::TextFormat;

    std::vector<ReadRowsResponse_CellChunk> chunks;
    for (std::string const& chunk_string : chunk_strings) {
      ReadRowsResponse_CellChunk chunk;
      if (not TextFormat::ParseFromString(chunk_string, &chunk)) {
        return {};
      }
      chunks.emplace_back(std::move(chunk));
    }

    return chunks;
  }

  void FeedChunks(std::vector<ReadRowsResponse_CellChunk> chunks) {
    for (auto const& chunk : chunks) {
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

// The tests included below are defined in the file
// "read-rows-acceptance-test.json" in the cloud-bigtable-client
// repository and formatted as C++ code using tools/convert_tests.py

#include "bigtable/client/detail/readrowsparser_acceptancetests.inc"
