// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may
// obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/row_stream.h"
#include "google/cloud/bigtable/mocks/mock_query_row.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <tuple>
#include <utility>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
using ::testing::Not;

// Given a `vector<StatusOr<QueryRow>>` creates a 'QueryRow::Source' object.
// This is helpful for unit testing and letting the test inject a non-OK Status.
RowStreamIterator::Source MakeRowStreamIteratorSource(
    std::vector<StatusOr<QueryRow>> const& rows) {
  std::size_t index = 0;
  return [=]() mutable -> StatusOr<QueryRow> {
    if (index == rows.size()) return QueryRow{};
    return rows[index++];
  };
}

// Given a `vector<QueryRow>` creates a 'QueryRow::Source' object.
RowStreamIterator::Source MakeRowStreamIteratorSource(
    std::vector<QueryRow> const& rows = {}) {
  return MakeRowStreamIteratorSource(
      std::vector<StatusOr<QueryRow>>(rows.begin(), rows.end()));
}

class QueryRowRange {
 public:
  explicit QueryRowRange(RowStreamIterator::Source s) : s_(std::move(s)) {}
  RowStreamIterator begin() { return RowStreamIterator(s_); }
  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  RowStreamIterator end() { return RowStreamIterator(); }

 private:
  RowStreamIterator::Source s_;
};

TEST(RowStreamIterator, Basics) {
  RowStreamIterator end;
  EXPECT_EQ(end, end);

  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow(1, "foo", true));
  rows.emplace_back(bigtable_mocks::MakeQueryRow(2, "bar", true));
  rows.emplace_back(bigtable_mocks::MakeQueryRow(3, "baz", true));

  auto it = RowStreamIterator(MakeRowStreamIteratorSource(rows));
  EXPECT_EQ(it, it);
  EXPECT_NE(it, end);
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ(rows[0], **it);

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_NE(it, end);
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ(rows[1], **it);

  it++;
  EXPECT_EQ(it, it);
  EXPECT_NE(it, end);
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ(rows[2], **it);

  // Tests op*() const and op->() const
  auto const copy = it;
  EXPECT_EQ(copy, it);
  EXPECT_NE(copy, end);
  ASSERT_STATUS_OK(*copy);
  EXPECT_STATUS_OK(copy->status());

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_EQ(it, end);
}

TEST(RowStreamIterator, Empty) {
  RowStreamIterator end;
  auto it = RowStreamIterator(MakeRowStreamIteratorSource());
  EXPECT_EQ(it, end);
}

TEST(RowStreamIterator, OneRow) {
  RowStreamIterator end;
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow(1, "foo", true));
  auto it = RowStreamIterator(MakeRowStreamIteratorSource(rows));
  EXPECT_NE(it, end);
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ(rows[0], **it);

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_EQ(it, end);
}

TEST(RowStreamIterator, IterationError) {
  RowStreamIterator end;
  std::vector<StatusOr<QueryRow>> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow(1, "foo", true));
  rows.emplace_back(Status(StatusCode::kUnknown, "some error"));
  rows.emplace_back(bigtable_mocks::MakeQueryRow(2, "bar", true));

  auto it = RowStreamIterator(MakeRowStreamIteratorSource(rows));
  ASSERT_NE(it, end);
  EXPECT_STATUS_OK(*it);
  EXPECT_EQ(rows[0], *it);

  ++it;
  EXPECT_EQ(it, it);
  ASSERT_NE(it, end);
  EXPECT_FALSE(*it);
  EXPECT_THAT(*it, StatusIs(StatusCode::kUnknown, "some error"));

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_EQ(it, end);
}

TEST(RowStreamIterator, ForLoop) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(2)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(3)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(5)}}));

  auto source = MakeRowStreamIteratorSource(rows);
  std::int64_t product = 1;
  for (RowStreamIterator it(source), end; it != end; ++it) {
    ASSERT_STATUS_OK(*it);
    auto num = (*it)->get<std::int64_t>("num");
    ASSERT_STATUS_OK(num);
    product *= *num;
  }
  EXPECT_EQ(product, 30);
}

TEST(RowStreamIterator, RangeForLoopFloat32) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(2.1F)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(3.2F)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(5.4F)}}));

  QueryRowRange range(MakeRowStreamIteratorSource(rows));
  float sum = 0;
  for (auto const& row : range) {
    ASSERT_STATUS_OK(row);
    auto num = row->get<float>("num");
    ASSERT_STATUS_OK(num);
    sum += *num;
  }
  EXPECT_FLOAT_EQ(sum, 10.7F);
}

TEST(RowStreamIterator, RangeForLoop) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(2)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(3)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(5)}}));

  QueryRowRange range(MakeRowStreamIteratorSource(rows));
  std::int64_t product = 1;
  for (auto const& row : range) {
    ASSERT_STATUS_OK(row);
    auto num = row->get<std::int64_t>("num");
    ASSERT_STATUS_OK(num);
    product *= *num;
  }
  EXPECT_EQ(product, 30);
}

TEST(RowStreamIterator, MovedFromValueOk) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(1)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(2)}}));

  QueryRowRange range(MakeRowStreamIteratorSource(rows));
  auto it = range.begin();
  auto end = range.end();

  EXPECT_NE(it, end);
  auto row = std::move(*it);
  ASSERT_STATUS_OK(row);
  auto val = row->get("num");
  ASSERT_STATUS_OK(val);
  EXPECT_EQ(Value(1), *val);

  ++it;
  EXPECT_NE(it, end);
  row = std::move(*it);
  ASSERT_STATUS_OK(row);
  val = row->get("num");
  ASSERT_STATUS_OK(val);
  EXPECT_EQ(Value(2), *val);

  ++it;
  EXPECT_EQ(it, end);
}

TEST(TupleStreamIterator, Basics) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow(1, "foo", true));
  rows.emplace_back(bigtable_mocks::MakeQueryRow(2, "bar", true));
  rows.emplace_back(bigtable_mocks::MakeQueryRow(3, "baz", true));

  using RowType = std::tuple<std::int64_t, std::string, bool>;
  using TupleIterator = TupleStreamIterator<RowType>;

  auto end = TupleIterator();
  EXPECT_EQ(end, end);

  auto it = TupleIterator(RowStreamIterator(MakeRowStreamIteratorSource(rows)),
                          RowStreamIterator());

  EXPECT_EQ(it, it);
  ASSERT_NE(it, end);
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ(std::make_tuple(1, "foo", true), **it);

  ++it;
  EXPECT_EQ(it, it);
  ASSERT_NE(it, end);
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ(std::make_tuple(2, "bar", true), **it);

  ++it;
  EXPECT_EQ(it, it);
  ASSERT_NE(it, end);
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ(std::make_tuple(3, "baz", true), **it);

  // Tests op*() const and op->() const
  auto const copy = it;
  EXPECT_EQ(copy, it);
  ASSERT_NE(copy, end);
  ASSERT_STATUS_OK(*copy);
  EXPECT_STATUS_OK(copy->status());

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_EQ(it, end);
}

TEST(TupleStreamIterator, Empty) {
  using RowType = std::tuple<std::int64_t, std::string, bool>;
  using TupleIterator = TupleStreamIterator<RowType>;

  auto end = TupleIterator();
  EXPECT_EQ(end, end);

  auto it = TupleIterator(RowStreamIterator(MakeRowStreamIteratorSource()),
                          RowStreamIterator());
  EXPECT_EQ(it, end);
}

TEST(TupleStreamIterator, Error) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow(1, "foo", true));
  rows.emplace_back(bigtable_mocks::MakeQueryRow(2, "bar", "should be a bool"));
  rows.emplace_back(bigtable_mocks::MakeQueryRow(3, "baz", true));

  using RowType = std::tuple<std::int64_t, std::string, bool>;
  using TupleIterator = TupleStreamIterator<RowType>;

  auto end = TupleIterator();
  EXPECT_EQ(end, end);

  auto it = TupleIterator(RowStreamIterator(MakeRowStreamIteratorSource(rows)),
                          RowStreamIterator());

  EXPECT_EQ(it, it);
  ASSERT_NE(it, end);
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ(std::make_tuple(1, "foo", true), **it);

  ++it;
  EXPECT_EQ(it, it);
  ASSERT_NE(it, end);
  EXPECT_THAT(*it, Not(IsOk()));  // Error parsing the 2nd element

  ++it;  // Due to the previous error, jumps straight to "end"
  EXPECT_EQ(it, it);
  EXPECT_EQ(it, end);
}

TEST(TupleStreamIterator, MovedFromValueOk) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(1)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(2)}}));

  QueryRowRange range(MakeRowStreamIteratorSource(rows));
  using RowType = std::tuple<std::int64_t>;
  using TupleIterator = TupleStreamIterator<RowType>;
  auto it = TupleIterator(range.begin(), range.end());
  auto end = TupleIterator();

  EXPECT_NE(it, end);
  auto tup = std::move(*it);
  ASSERT_STATUS_OK(tup);
  EXPECT_EQ(1, std::get<0>(*tup));

  ++it;
  ASSERT_NE(it, end);
  tup = std::move(*it);
  ASSERT_STATUS_OK(tup);
  EXPECT_EQ(2, std::get<0>(*tup));

  ++it;
  EXPECT_EQ(it, end);
}

TEST(TupleStream, Basics) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow(1, "foo", true));
  rows.emplace_back(bigtable_mocks::MakeQueryRow(2, "bar", true));
  rows.emplace_back(bigtable_mocks::MakeQueryRow(3, "baz", true));

  using RowType = std::tuple<std::int64_t, std::string, bool>;
  QueryRowRange range(MakeRowStreamIteratorSource(rows));
  auto parser = StreamOf<RowType>(range);
  auto it = parser.begin();
  auto end = parser.end();
  EXPECT_EQ(end, end);

  EXPECT_EQ(it, it);
  ASSERT_NE(it, end);
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ(std::make_tuple(1, "foo", true), **it);

  ++it;
  EXPECT_EQ(it, it);
  ASSERT_NE(it, end);
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ(std::make_tuple(2, "bar", true), **it);

  ++it;
  EXPECT_EQ(it, it);
  ASSERT_NE(it, end);
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ(std::make_tuple(3, "baz", true), **it);

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_EQ(it, end);
}

TEST(TupleStream, RangeForLoop) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(2)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(3)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(5)}}));
  using RowType = std::tuple<std::int64_t>;

  QueryRowRange range(MakeRowStreamIteratorSource(rows));
  std::int64_t product = 1;
  for (auto const& row : StreamOf<RowType>(range)) {
    ASSERT_STATUS_OK(row);
    product *= std::get<0>(*row);
  }
  EXPECT_EQ(product, 30);
}

TEST(TupleStream, IterationError) {
  std::vector<StatusOr<QueryRow>> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow(1, "foo", true));
  rows.emplace_back(Status(StatusCode::kUnknown, "some error"));
  rows.emplace_back(bigtable_mocks::MakeQueryRow(2, "bar", true));

  QueryRowRange range(MakeRowStreamIteratorSource(rows));

  using RowType = std::tuple<std::int64_t, std::string, bool>;
  auto stream = StreamOf<RowType>(range);

  auto end = stream.end();
  auto it = stream.begin();
  ASSERT_NE(it, end);
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ(std::make_tuple(1, "foo", true), **it);

  ++it;
  EXPECT_EQ(it, it);
  ASSERT_NE(it, end);
  EXPECT_FALSE(*it);
  EXPECT_THAT(*it, StatusIs(StatusCode::kUnknown, "some error"));

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_EQ(it, end);
}

TEST(GetSingularRow, BasicEmpty) {
  std::vector<QueryRow> rows;
  QueryRowRange range(MakeRowStreamIteratorSource(rows));
  auto row = GetSingularRow(range);
  EXPECT_THAT(row,
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("no rows")));
}

TEST(GetSingularRow, TupleStreamEmpty) {
  std::vector<QueryRow> rows;
  QueryRowRange range(MakeRowStreamIteratorSource(rows));
  auto row = GetSingularRow(StreamOf<std::tuple<std::int64_t>>(range));
  EXPECT_THAT(row,
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("no rows")));
}

TEST(GetSingularRow, BasicSingleRow) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(1)}}));

  QueryRowRange range(MakeRowStreamIteratorSource(rows));
  auto row = GetSingularRow(range);
  ASSERT_STATUS_OK(row);
  EXPECT_EQ(1, *row->get<std::int64_t>(0));
}

TEST(GetSingularRow, TupleStreamSingleRow) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(1)}}));

  auto row_range = QueryRowRange(MakeRowStreamIteratorSource(rows));
  auto tup_range = StreamOf<std::tuple<std::int64_t>>(row_range);

  auto row = GetSingularRow(tup_range);
  ASSERT_STATUS_OK(row);
  EXPECT_EQ(1, std::get<0>(*row));
}

TEST(GetSingularRow, BasicTooManyRows) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(1)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(2)}}));

  QueryRowRange range(MakeRowStreamIteratorSource(rows));
  auto row = GetSingularRow(range);
  EXPECT_THAT(
      row, StatusIs(StatusCode::kInvalidArgument, HasSubstr("too many rows")));
}

TEST(GetSingularRow, TupleStreamTooManyRows) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(1)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(2)}}));

  QueryRowRange range(MakeRowStreamIteratorSource(rows));
  auto row = GetSingularRow(StreamOf<std::tuple<std::int64_t>>(range));
  EXPECT_THAT(
      row, StatusIs(StatusCode::kInvalidArgument, HasSubstr("too many rows")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
