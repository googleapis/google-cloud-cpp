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

#include "bigtable/client/filters.h"

#include <gmock/gmock.h>

#include "bigtable/client/chrono_literals.h"

/// @test Verify that functions to create simple filters work as expected.
TEST(FiltersTest, Simple) {
  namespace btproto = ::google::bigtable::v2;

  auto proto = bigtable::Filter::Latest(3).as_proto();
  EXPECT_EQ(3, proto.cells_per_column_limit_filter());

  proto = bigtable::Filter::Column("col[A-E]").as_proto();
  EXPECT_EQ("col[A-E]", proto.column_qualifier_regex_filter());

  proto = bigtable::Filter::ColumnRange("colA", "colF").as_proto();
  ASSERT_EQ(btproto::ColumnRange::kStartQualifierClosed,
            proto.column_range_filter().start_qualifier_case());
  EXPECT_EQ("colA", proto.column_range_filter().start_qualifier_closed());
  ASSERT_EQ(btproto::ColumnRange::kEndQualifierOpen,
            proto.column_range_filter().end_qualifier_case());
  EXPECT_EQ("colF", proto.column_range_filter().end_qualifier_open());

  proto = bigtable::Filter::Family("fam[123]").as_proto();
  EXPECT_EQ("fam[123]", proto.family_name_regex_filter());

  proto = bigtable::Filter::TimestampRangeMicros(0, 10).as_proto();
  EXPECT_EQ(0, proto.timestamp_range_filter().start_timestamp_micros());
  EXPECT_EQ(10, proto.timestamp_range_filter().end_timestamp_micros());

  using namespace bigtable::chrono_literals;
  proto = bigtable::Filter::TimestampRange(10_us, 10_ms).as_proto();
  EXPECT_EQ(10, proto.timestamp_range_filter().start_timestamp_micros());
  EXPECT_EQ(10000, proto.timestamp_range_filter().end_timestamp_micros());

  proto = bigtable::Filter::MatchingRowKeys("[A-Za-z][A-Za-z0-9_]*").as_proto();
  EXPECT_EQ("[A-Za-z][A-Za-z0-9_]*", proto.row_key_regex_filter());

  proto = bigtable::Filter::PassAllFilter().as_proto();
  EXPECT_TRUE(proto.pass_all_filter());

  proto = bigtable::Filter::BlockAllFilter().as_proto();
  EXPECT_TRUE(proto.block_all_filter());

  proto = bigtable::Filter::CellsRowLimit(3).as_proto();
  EXPECT_EQ(3, proto.cells_per_row_limit_filter());

  proto = bigtable::Filter::CellsRowOffset(42).as_proto();
  EXPECT_EQ(42, proto.cells_per_row_offset_filter());
}

/// @test Verify that functions to create transformer filters work as expected.
TEST(FiltersTest, Transformers) {
  namespace btproto = ::google::bigtable::v2;

  auto proto = bigtable::Filter::StripValueTransformer().as_proto();
  EXPECT_TRUE(proto.strip_value_transformer());

  proto = bigtable::Filter::ApplyLabelTransformer("foo").as_proto();
  EXPECT_EQ("foo", proto.apply_label_transformer());
}
