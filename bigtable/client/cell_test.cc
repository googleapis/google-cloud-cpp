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

  std::vector<absl::string_view> chunks = {"one", "two"};
  std::vector<absl::string_view> labels;

  bigtable::Cell cell(row_key, family_name, column_qualifier, timestamp, chunks,
                      labels);
  EXPECT_EQ(row_key, cell.row_key());
  EXPECT_EQ(family_name, cell.family_name());
  EXPECT_EQ(column_qualifier, cell.column_qualifier());
  EXPECT_EQ(timestamp, cell.timestamp());
  EXPECT_EQ("onetwo", cell.value());
  EXPECT_EQ(0u, cell.labels().size());
}

/// @test Verify consolidation of different chunks corner cases.
TEST(CellTest, Consolidation) {
  bigtable::Cell no_chunks("r", "f", "c", 42, {}, {});
  EXPECT_EQ("", no_chunks.value());

  bigtable::Cell empty_chunk("r", "f", "c", 42, {""}, {});
  EXPECT_EQ("", empty_chunk.value());

  bigtable::Cell two_empty_chunks("r", "f", "c", 42, {"", ""}, {});
  EXPECT_EQ("", two_empty_chunks.value());

  bigtable::Cell one_chunk("r", "f", "c", 42, {"one"}, {});
  EXPECT_EQ("one", one_chunk.value());

  bigtable::Cell two_chunks("r", "f", "c", 42, {"one", "two"}, {});
  EXPECT_EQ("onetwo", two_chunks.value());

  bigtable::Cell empty_chunk_sandwish("r", "f", "c", 42,
                                      {"", "one", "", "two", ""}, {});
  EXPECT_EQ("onetwo", empty_chunk_sandwish.value());
}

/// @test Verify that the cell value is stable.
TEST(CellTest, ValueIsStable) {
  bigtable::Cell two_chunks("r", "f", "c", 42, {"one", "two"}, {});
  auto value = two_chunks.value();
  EXPECT_EQ("onetwo", value);
  // The first time value() is called, it caches the concatenated string with
  // all chunks. When calling it a couple of extra times, we expect the
  // string_view to be backed by the same buffer.
  EXPECT_EQ(value, two_chunks.value());
  EXPECT_EQ(value.data(), two_chunks.value().data());

  // The same applies when chunks are empty or none at all.
  bigtable::Cell two_empty_chunks("r", "f", "c", 42, {"", ""}, {});
  auto empty_value = two_empty_chunks.value();
  EXPECT_EQ("", empty_value);
  EXPECT_EQ(empty_value, two_empty_chunks.value());
  EXPECT_EQ(empty_value.data(), two_empty_chunks.value().data());

  bigtable::Cell no_chunks("r", "f", "c", 42, {}, {});
  auto no_value = no_chunks.value();
  EXPECT_EQ("", no_value);
  EXPECT_EQ(no_value, no_chunks.value());
  EXPECT_EQ(no_value.data(), no_chunks.value().data());
}
