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

#include "google/cloud/bigtable/emulator/row_iterators.h"
#include "google/cloud/bigtable/row_range.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {
namespace {

using namespace std::chrono_literals;

TEST(ColumnFamilyIterator, Simple) {
  ColumnFamily fam;
  fam.SetCell("row1", "col1", 123ms, "foo");
  fam.SetCell("row1", "col1", 124ms, "fo");
  fam.SetCell("row1", "col2", 123ms, "bar");
  fam.SetCell("row2", "col1", 123ms, "foo");
  fam.SetCell("row2", "col3", 120ms, "baz");
  fam.SetCell("row2", "col3", 120ms, "baz");
  std::vector<std::string> rows;
  std::transform(
      fam.FindRows(std::shared_ptr<SortedRowSet>(
          new SortedRowSet(SortedRowSet::AllRows()))),
      fam.end(),
      std::back_inserter(rows),
      [](std::pair<std::string const, ColumnFamilyRow const&> const& val) {
        return val.first;
      });
  std::vector<std::string> expected{"row1", "row2"};
  EXPECT_EQ(expected, rows);
}

class Foo {
 public:
  Foo(std::string const& foo) : foo_(foo) {}

 private:
  std::reference_wrapper<std::string const> foo_;
};

}  // anonymous namespace
}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google


