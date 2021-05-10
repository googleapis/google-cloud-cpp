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

#include "google/cloud/bigtable/filters.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

namespace btproto = ::google::bigtable::v2;

using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::chrono_literals::operator"" _ms;
using ::google::cloud::testing_util::chrono_literals::operator"" _us;

TEST(FiltersTest, PassAllFilter) {
  auto proto = Filter::PassAllFilter().as_proto();
  EXPECT_TRUE(proto.pass_all_filter());
}

TEST(FiltersTest, BlockAllFilter) {
  auto proto = Filter::BlockAllFilter().as_proto();
  EXPECT_TRUE(proto.block_all_filter());
}

TEST(FiltersTest, Latest) {
  auto proto = Filter::Latest(3).as_proto();
  EXPECT_EQ(3, proto.cells_per_column_limit_filter());
}

TEST(FiltersTest, FamilyRegex) {
  auto proto = Filter::FamilyRegex("fam[123]").as_proto();
  EXPECT_EQ("fam[123]", proto.family_name_regex_filter());
}

TEST(FiltersTest, ColumnRegex) {
  auto proto = Filter::ColumnRegex("col[A-E]").as_proto();
  EXPECT_EQ("col[A-E]", proto.column_qualifier_regex_filter());
}

TEST(FiltersTest, ColumnRange) {
  auto proto = Filter::ColumnRange("fam", "colA", "colF").as_proto();
  EXPECT_EQ("fam", proto.column_range_filter().family_name());
  EXPECT_EQ(btproto::ColumnRange::kStartQualifierClosed,
            proto.column_range_filter().start_qualifier_case());
  EXPECT_EQ("colA", proto.column_range_filter().start_qualifier_closed());
  EXPECT_EQ(btproto::ColumnRange::kEndQualifierOpen,
            proto.column_range_filter().end_qualifier_case());
  EXPECT_EQ("colF", proto.column_range_filter().end_qualifier_open());
}

TEST(FiltersTest, ColumnName) {
  auto proto = Filter::ColumnName("fam", "colA").as_proto();
  EXPECT_EQ("fam", proto.column_range_filter().family_name());
  EXPECT_EQ("colA", proto.column_range_filter().start_qualifier_closed());
  EXPECT_EQ("colA", proto.column_range_filter().end_qualifier_closed());
}

TEST(FiltersTest, TimestampRangeMicros) {
  auto proto = Filter::TimestampRangeMicros(0, 10).as_proto();
  EXPECT_EQ(0, proto.timestamp_range_filter().start_timestamp_micros());
  EXPECT_EQ(10, proto.timestamp_range_filter().end_timestamp_micros());
}

TEST(FiltersTest, TimestampRange) {
  auto proto = Filter::TimestampRange(10_us, 10_ms).as_proto();
  EXPECT_EQ(10, proto.timestamp_range_filter().start_timestamp_micros());
  EXPECT_EQ(10000, proto.timestamp_range_filter().end_timestamp_micros());
}

TEST(FiltersTest, RowKeysRegex) {
  auto proto = Filter::RowKeysRegex("[A-Za-z][A-Za-z0-9_]*").as_proto();
  EXPECT_EQ("[A-Za-z][A-Za-z0-9_]*", proto.row_key_regex_filter());
}

TEST(FiltersTest, CellsRowLimit) {
  auto proto = Filter::CellsRowLimit(3).as_proto();
  EXPECT_EQ(3, proto.cells_per_row_limit_filter());
}

TEST(FiltersTest, ValueRegex) {
  auto proto = Filter::ValueRegex("foo:\\n  'bar.*'").as_proto();
  EXPECT_EQ("foo:\\n  'bar.*'", proto.value_regex_filter());
}

TEST(FiltersTest, CellsRowOffset) {
  auto proto = Filter::CellsRowOffset(42).as_proto();
  EXPECT_EQ(42, proto.cells_per_row_offset_filter());
}

TEST(FiltersTEst, RowSample) {
  auto proto = Filter::RowSample(0.5).as_proto();
  ASSERT_DOUBLE_EQ(0.5, proto.row_sample_filter());
}

TEST(FiltersTest, ValueRangeLeftOpen) {
  auto proto = Filter::ValueRangeLeftOpen("2017-02", "2017-09").as_proto();
  ASSERT_EQ(btproto::ValueRange::kStartValueOpen,
            proto.value_range_filter().start_value_case());
  ASSERT_EQ(btproto::ValueRange::kEndValueClosed,
            proto.value_range_filter().end_value_case());
  EXPECT_EQ("2017-02", proto.value_range_filter().start_value_open());
  EXPECT_EQ("2017-09", proto.value_range_filter().end_value_closed());
}

TEST(FiltersTest, ValueRangeRightOpen) {
  auto proto = Filter::ValueRangeRightOpen("2017", "2018").as_proto();
  ASSERT_EQ(btproto::ValueRange::kStartValueClosed,
            proto.value_range_filter().start_value_case());
  ASSERT_EQ(btproto::ValueRange::kEndValueOpen,
            proto.value_range_filter().end_value_case());
  EXPECT_EQ("2017", proto.value_range_filter().start_value_closed());
  EXPECT_EQ("2018", proto.value_range_filter().end_value_open());
}

TEST(FiltersTest, ValueRangeClosed) {
  auto proto = Filter::ValueRangeClosed("2017", "2018").as_proto();
  ASSERT_EQ(btproto::ValueRange::kStartValueClosed,
            proto.value_range_filter().start_value_case());
  ASSERT_EQ(btproto::ValueRange::kEndValueClosed,
            proto.value_range_filter().end_value_case());
  EXPECT_EQ("2017", proto.value_range_filter().start_value_closed());
  EXPECT_EQ("2018", proto.value_range_filter().end_value_closed());
}

TEST(FiltersTest, ValueRangeOpen) {
  auto proto = Filter::ValueRangeOpen("2016", "2019").as_proto();
  ASSERT_EQ(btproto::ValueRange::kStartValueOpen,
            proto.value_range_filter().start_value_case());
  ASSERT_EQ(btproto::ValueRange::kEndValueOpen,
            proto.value_range_filter().end_value_case());
  EXPECT_EQ("2016", proto.value_range_filter().start_value_open());
  EXPECT_EQ("2019", proto.value_range_filter().end_value_open());
}

TEST(FiltersTest, ColumnRangeRightOpen) {
  auto proto = Filter::ColumnRangeRightOpen("fam", "col1", "col3").as_proto();
  ASSERT_EQ(btproto::ColumnRange::kStartQualifierClosed,
            proto.column_range_filter().start_qualifier_case());
  ASSERT_EQ(btproto::ColumnRange::kEndQualifierOpen,
            proto.column_range_filter().end_qualifier_case());
  EXPECT_EQ("col1", proto.column_range_filter().start_qualifier_closed());
  EXPECT_EQ("col3", proto.column_range_filter().end_qualifier_open());
  EXPECT_EQ("fam", proto.column_range_filter().family_name());
}

TEST(FiltersTest, ColumnRangeLeftOpen) {
  auto proto = Filter::ColumnRangeLeftOpen("fam", "col1", "col3").as_proto();
  ASSERT_EQ(btproto::ColumnRange::kStartQualifierOpen,
            proto.column_range_filter().start_qualifier_case());
  ASSERT_EQ(btproto::ColumnRange::kEndQualifierClosed,
            proto.column_range_filter().end_qualifier_case());
  EXPECT_EQ("col1", proto.column_range_filter().start_qualifier_open());
  EXPECT_EQ("col3", proto.column_range_filter().end_qualifier_closed());
  EXPECT_EQ("fam", proto.column_range_filter().family_name());
}

TEST(FiltersTest, ColumnRangeClosed) {
  auto proto = Filter::ColumnRangeClosed("fam", "col1", "col3").as_proto();
  ASSERT_EQ(btproto::ColumnRange::kStartQualifierClosed,
            proto.column_range_filter().start_qualifier_case());
  ASSERT_EQ(btproto::ColumnRange::kEndQualifierClosed,
            proto.column_range_filter().end_qualifier_case());
  EXPECT_EQ("col1", proto.column_range_filter().start_qualifier_closed());
  EXPECT_EQ("col3", proto.column_range_filter().end_qualifier_closed());
  EXPECT_EQ("fam", proto.column_range_filter().family_name());
}

TEST(FiltersTest, ColumnRangeOpen) {
  auto proto = Filter::ColumnRangeOpen("fam", "col1", "col3").as_proto();
  ASSERT_EQ(btproto::ColumnRange::kStartQualifierOpen,
            proto.column_range_filter().start_qualifier_case());
  ASSERT_EQ(btproto::ColumnRange::kEndQualifierOpen,
            proto.column_range_filter().end_qualifier_case());
  EXPECT_EQ("col1", proto.column_range_filter().start_qualifier_open());
  EXPECT_EQ("col3", proto.column_range_filter().end_qualifier_open());
  EXPECT_EQ("fam", proto.column_range_filter().family_name());
}

TEST(FiltersTest, StripValueTransformer) {
  auto proto = Filter::StripValueTransformer().as_proto();
  EXPECT_TRUE(proto.strip_value_transformer());
}

TEST(FiltersTest, ApplyLabelTransformer) {
  auto proto = Filter::ApplyLabelTransformer("foo").as_proto();
  EXPECT_EQ("foo", proto.apply_label_transformer());
}

/// @test Verify that `bigtable::Filter::Condition` works as expected.
TEST(FiltersTest, Condition) {
  using F = Filter;
  auto filter = F::Condition(F::ColumnRegex("foo"), F::CellsRowLimit(1),
                             F::CellsRowOffset(2));
  auto const& proto = filter.as_proto();
  ASSERT_TRUE(proto.has_condition());
  auto const& predicate = proto.condition().predicate_filter();
  EXPECT_EQ("foo", predicate.column_qualifier_regex_filter());
  auto const& true_f = proto.condition().true_filter();
  EXPECT_EQ(1, true_f.cells_per_row_limit_filter());
  auto const& false_f = proto.condition().false_filter();
  EXPECT_EQ(2, false_f.cells_per_row_offset_filter());
}

/// @test Verify that `bigtable::Filter::Chain` works as expected.
TEST(FiltersTest, ChainMultipleArgs) {
  using F = Filter;
  auto filter = F::Chain(F::FamilyRegex("fam"), F::ColumnRegex("col"),
                         F::CellsRowOffset(2), F::Latest(1));
  auto const& proto = filter.as_proto();
  ASSERT_TRUE(proto.has_chain());
  auto const& chain = proto.chain();
  ASSERT_EQ(4, chain.filters_size());
  EXPECT_EQ("fam", chain.filters(0).family_name_regex_filter());
  EXPECT_EQ("col", chain.filters(1).column_qualifier_regex_filter());
  EXPECT_EQ(2, chain.filters(2).cells_per_row_offset_filter());
  EXPECT_EQ(1, chain.filters(3).cells_per_column_limit_filter());
}

/// @test Verify that `bigtable::Filter::Chain` works as expected.
TEST(FiltersTest, ChainNoArgs) {
  using F = Filter;
  auto filter = F::Chain();
  auto const& proto = filter.as_proto();
  ASSERT_TRUE(proto.has_chain());
  auto const& chain = proto.chain();
  ASSERT_EQ(0, chain.filters_size());
}

/// @test Verify that `bigtable::Filter::Chain` works as expected.
TEST(FiltersTest, ChainOneArg) {
  using F = Filter;
  auto filter = F::Chain(F::Latest(2));
  auto const& proto = filter.as_proto();
  ASSERT_TRUE(proto.has_chain());
  auto const& chain = proto.chain();
  ASSERT_EQ(1, chain.filters_size());
  EXPECT_EQ(2, chain.filters(0).cells_per_column_limit_filter());
}

/// @test Verify that `bigtable::Filter::ChainFromRange` works as expected.
TEST(FiltersTest, ChainFromRangeMany) {
  using F = Filter;
  std::vector<F> filter_collection{F::FamilyRegex("fam"), F::ColumnRegex("col"),
                                   F::CellsRowOffset(2), F::Latest(1)};
  auto filter =
      F::ChainFromRange(filter_collection.begin(), filter_collection.end());
  auto const& proto = filter.as_proto();
  ASSERT_TRUE(proto.has_chain());
  auto const& chain = proto.chain();
  ASSERT_EQ(4, chain.filters_size());
  EXPECT_EQ("fam", chain.filters(0).family_name_regex_filter());
  EXPECT_EQ("col", chain.filters(1).column_qualifier_regex_filter());
  EXPECT_EQ(2, chain.filters(2).cells_per_row_offset_filter());
  EXPECT_EQ(1, chain.filters(3).cells_per_column_limit_filter());
}

/// @test Verify that `bigtable::Filter::ChainFromRange` works as expected.
TEST(FiltersTest, ChainFromRangeEmpty) {
  using F = Filter;
  std::vector<F> filter_collection{};
  auto filter =
      F::ChainFromRange(filter_collection.begin(), filter_collection.end());
  auto const& proto = filter.as_proto();
  ASSERT_TRUE(proto.has_chain());
  auto const& chain = proto.chain();
  ASSERT_EQ(0, chain.filters_size());
}

/// @test Verify that `bigtable::Filter::ChainFromRange` works as expected.
TEST(FiltersTest, ChainFromRangeSingle) {
  using F = Filter;
  std::vector<F> filter_collection{F::Latest(2)};
  auto filter =
      F::ChainFromRange(filter_collection.begin(), filter_collection.end());
  auto const& proto = filter.as_proto();
  ASSERT_TRUE(proto.has_chain());
  auto const& chain = proto.chain();
  ASSERT_EQ(1, chain.filters_size());
  EXPECT_EQ(2, chain.filters(0).cells_per_column_limit_filter());
}

/// @test Verify that `bigtable::Filter::Interleave` works as expected.
TEST(FiltersTest, InterleaveMultipleArgs) {
  using F = Filter;
  auto filter = F::Interleave(F::FamilyRegex("fam"), F::ColumnRegex("col"),
                              F::CellsRowOffset(2), F::Latest(1));
  auto const& proto = filter.as_proto();
  ASSERT_TRUE(proto.has_interleave());
  auto const& interleave = proto.interleave();
  ASSERT_EQ(4, interleave.filters_size());
  EXPECT_EQ("fam", interleave.filters(0).family_name_regex_filter());
  EXPECT_EQ("col", interleave.filters(1).column_qualifier_regex_filter());
  EXPECT_EQ(2, interleave.filters(2).cells_per_row_offset_filter());
  EXPECT_EQ(1, interleave.filters(3).cells_per_column_limit_filter());
}

/// @test Verify that `bigtable::Filter::Interleave` works as expected.
TEST(FiltersTest, InterleaveNoArgs) {
  using F = Filter;
  auto filter = F::Interleave();
  auto const& proto = filter.as_proto();
  ASSERT_TRUE(proto.has_interleave());
  auto const& interleave = proto.interleave();
  ASSERT_EQ(0, interleave.filters_size());
}

/// @test Verify that `bigtable::Filter::Interleave` works as expected.
TEST(FiltersTest, InterleaveOneArg) {
  using F = Filter;
  auto filter = F::Interleave(F::Latest(2));
  auto const& proto = filter.as_proto();
  ASSERT_TRUE(proto.has_interleave());
  auto const& interleave = proto.interleave();
  ASSERT_EQ(1, interleave.filters_size());
  EXPECT_EQ(2, interleave.filters(0).cells_per_column_limit_filter());
}

/// @test Verify that `bigtable::Filter::InterleaveFromRange` works as expected.
TEST(FiltersTest, InterleaveFromRangeMany) {
  using F = Filter;
  std::vector<F> filter_collection{F::FamilyRegex("fam"), F::ColumnRegex("col"),
                                   F::CellsRowOffset(2), F::Latest(1)};
  auto filter = F::InterleaveFromRange(filter_collection.begin(),
                                       filter_collection.end());
  auto const& proto = filter.as_proto();
  ASSERT_TRUE(proto.has_interleave());
  auto const& interleave = proto.interleave();
  ASSERT_EQ(4, interleave.filters_size());
  EXPECT_EQ("fam", interleave.filters(0).family_name_regex_filter());
  EXPECT_EQ("col", interleave.filters(1).column_qualifier_regex_filter());
  EXPECT_EQ(2, interleave.filters(2).cells_per_row_offset_filter());
  EXPECT_EQ(1, interleave.filters(3).cells_per_column_limit_filter());
}

/// @test Verify that `bigtable::Filter::InterleaveFromRange` works as expected.
TEST(FiltersTest, InterleaveFromRangeEmpty) {
  using F = Filter;
  std::vector<F> filter_collection{};
  auto filter = F::InterleaveFromRange(filter_collection.begin(),
                                       filter_collection.end());
  auto const& proto = filter.as_proto();
  ASSERT_TRUE(proto.has_interleave());
  auto const& interleave = proto.interleave();
  ASSERT_EQ(0, interleave.filters_size());
}

/// @test Verify that `bigtable::Filter::InterleaveFromRange` works as expected.
TEST(FiltersTest, InterleaveFromRangeSingle) {
  using F = Filter;
  std::vector<F> filter_collection{F::Latest(2)};
  auto filter = F::InterleaveFromRange(filter_collection.begin(),
                                       filter_collection.end());
  auto const& proto = filter.as_proto();
  ASSERT_TRUE(proto.has_interleave());
  auto const& interleave = proto.interleave();
  ASSERT_EQ(1, interleave.filters_size());
  EXPECT_EQ(2, interleave.filters(0).cells_per_column_limit_filter());
}

/// @test Verify that `bigtable::Filter::Sink` works as expected.
TEST(FiltersTest, Sink) {
  auto filter = Filter::Sink();
  auto const& proto = filter.as_proto();
  ASSERT_TRUE(proto.sink());
}

/// @test Verify that `bigtable::Filter::as_proto` works as expected.
TEST(FiltersTest, MoveProto) {
  using F = Filter;
  auto filter = F::Chain(F::FamilyRegex("fam"), F::ColumnRegex("col"),
                         F::CellsRowOffset(2), F::Latest(1));
  auto proto_copy = filter.as_proto();
  auto proto_move = std::move(filter).as_proto();
  // NOLINTNEXTLINE(bugprone-use-after-move)
  ASSERT_FALSE(filter.as_proto().has_chain());

  // Verify that as_proto() for rvalue-references returns the right type.
  static_assert(std::is_rvalue_reference<
                    decltype(std::move(std::declval<F>()).as_proto())>::value,
                "Return type from as_proto() must be rvalue-reference");

  EXPECT_THAT(proto_copy, IsProtoEqual(proto_move));
}

/// @test Verify that the ctor receiving a v2 RowFilter works as expected.
TEST(FiltersTest, RowFilterCtor) {
  using F = Filter;
  // We use a simple filter just as a confidence check.
  btproto::RowFilter row_filter;
  row_filter.set_row_key_regex_filter("[A-Za-z][A-Za-z0-9_]*");
  auto filter = F(row_filter);
  EXPECT_THAT(row_filter, IsProtoEqual(filter.as_proto()));
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
