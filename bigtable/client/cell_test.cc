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

#include "bigtable/client/cell.h"

#include <gtest/gtest.h>

/// @test Verify Cell instantiation and trivial accessors.
TEST(CellTest, Simple) {
  using namespace ::testing;

  std::string row_key = "row";
  std::string family_name = "family";
  std::string column_qualifier = "column";
  int64_t timestamp = 42;
  std::string value = "value";
  std::vector<std::string> labels;

  bigtable::Cell cell(row_key, family_name, column_qualifier, timestamp, value,
                      labels);
  EXPECT_EQ(row_key, cell.row_key());
  EXPECT_EQ(family_name, cell.family_name());
  EXPECT_EQ(column_qualifier, cell.column_qualifier());
  EXPECT_EQ(timestamp, cell.timestamp());
  EXPECT_EQ(value, cell.value());
  EXPECT_EQ(0U, cell.labels().size());
}
