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

#include "bigtable/client/row.h"

#include <gtest/gtest.h>

/// @test Verify Row instantiation and trivial accessors.
TEST(RowTest, Simple) {
  using namespace ::testing;

  std::string row_key = "row";

  bigtable::Cell cell(row_key, "family", "column", 42, "value", {});
  bigtable::Row row(row_key, {cell});

  EXPECT_EQ(row_key, cell.row_key());
  EXPECT_EQ(1u, row.cells().size());
  EXPECT_EQ(row_key, row.cells().begin()->row_key());
}
