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

#include "google/cloud/bigtable/query_row.h"
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
QueryRowStreamIterator::Source MakeQueryRowStreamIteratorSource(
    std::vector<StatusOr<QueryRow>> const& rows) {
  std::size_t index = 0;
  return [=]() mutable -> StatusOr<QueryRow> {
    if (index == rows.size()) return QueryRow{};
    return rows[index++];
  };
}

// Given a `vector<QueryRow>` creates a 'QueryRow::Source' object.
QueryRowStreamIterator::Source MakeQueryRowStreamIteratorSource(
    std::vector<QueryRow> const& rows = {}) {
  return MakeQueryRowStreamIteratorSource(
      std::vector<StatusOr<QueryRow>>(rows.begin(), rows.end()));
}

class QueryRowRange {
 public:
  explicit QueryRowRange(QueryRowStreamIterator::Source s) : s_(std::move(s)) {}
  QueryRowStreamIterator begin() { return QueryRowStreamIterator(s_); }
  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  QueryRowStreamIterator end() { return QueryRowStreamIterator(); }

 private:
  QueryRowStreamIterator::Source s_;
};

TEST(QueryRow, DefaultConstruct) {
  QueryRow row;
  EXPECT_EQ(0, row.size());
}

TEST(QueryRow, ValueSemantics) {
  QueryRow row = bigtable_mocks::MakeQueryRow(1, "blah", true);

  QueryRow copy = row;
  EXPECT_EQ(copy, row);

  copy = row;
  EXPECT_EQ(copy, row);

  QueryRow move = std::move(row);
  EXPECT_EQ(move, copy);

  row = copy;
  move = std::move(row);
  EXPECT_EQ(move, copy);
}

TEST(QueryRow, BasicAccessors) {
  auto values = std::vector<Value>{Value(1), Value("blah"), Value(true)};
  auto columns = std::vector<std::string>{"a", "b", "c"};
  QueryRow row = bigtable_mocks::MakeQueryRow({{columns[0], values[0]},
                                               {columns[1], values[1]},
                                               {columns[2], values[2]}});

  EXPECT_EQ(3, row.size());
  EXPECT_EQ(values, row.values());
  EXPECT_EQ(columns, row.columns());
  EXPECT_EQ(values, std::move(row).values());
}

TEST(QueryRow, GetByPosition) {
  QueryRow row = bigtable_mocks::MakeQueryRow(1, "blah", true);

  EXPECT_STATUS_OK(row.get(0));
  EXPECT_STATUS_OK(row.get(1));
  EXPECT_STATUS_OK(row.get(2));
  EXPECT_THAT(row.get(3), Not(IsOk()));

  EXPECT_EQ(Value(1), *row.get(0));
  EXPECT_EQ(Value("blah"), *row.get(1));
  EXPECT_EQ(Value(true), *row.get(2));
}

TEST(QueryRow, GetByColumnName) {
  QueryRow row = bigtable_mocks::MakeQueryRow(
      {{"a", Value(1)}, {"b", Value("blah")}, {"c", Value(true)}});

  EXPECT_STATUS_OK(row.get("a"));
  EXPECT_STATUS_OK(row.get("b"));
  EXPECT_STATUS_OK(row.get("c"));
  EXPECT_THAT(row.get("not a column name"), Not(IsOk()));

  EXPECT_EQ(Value(1), *row.get("a"));
  EXPECT_EQ(Value("blah"), *row.get("b"));
  EXPECT_EQ(Value(true), *row.get("c"));
}

TEST(QueryRow, TemplatedGetByPosition) {
  QueryRow row = bigtable_mocks::MakeQueryRow(1, "blah", true);

  EXPECT_STATUS_OK(row.get<std::int64_t>(0));
  EXPECT_STATUS_OK(row.get<std::string>(1));
  EXPECT_STATUS_OK(row.get<bool>(2));

  // Ensures that the wrong type specification results in a failure.
  EXPECT_THAT(row.get<bool>(0), Not(IsOk()));
  EXPECT_THAT(row.get<std::int64_t>(1), Not(IsOk()));
  EXPECT_THAT(row.get<std::string>(2), Not(IsOk()));
  EXPECT_THAT(row.get<std::int64_t>(3), Not(IsOk()));

  EXPECT_EQ(1, *row.get<std::int64_t>(0));
  EXPECT_EQ("blah", *row.get<std::string>(1));
  EXPECT_EQ(true, *row.get<bool>(2));
}

TEST(QueryRow, TemplatedGetByColumnName) {
  QueryRow row = bigtable_mocks::MakeQueryRow(
      {{"a", Value(1)}, {"b", Value("blah")}, {"c", Value(true)}});

  EXPECT_STATUS_OK(row.get<std::int64_t>("a"));
  EXPECT_STATUS_OK(row.get<std::string>("b"));
  EXPECT_STATUS_OK(row.get<bool>("c"));

  // Ensures that the wrong type specification results in a failure.
  EXPECT_THAT(row.get<bool>("a"), Not(IsOk()));
  EXPECT_THAT(row.get<std::int64_t>("b"), Not(IsOk()));
  EXPECT_THAT(row.get<std::string>("c"), Not(IsOk()));
  EXPECT_THAT(row.get<std::string>("column does not exist"), Not(IsOk()));

  EXPECT_EQ(1, *row.get<std::int64_t>("a"));
  EXPECT_EQ("blah", *row.get<std::string>("b"));
  EXPECT_EQ(true, *row.get<bool>("c"));
}

TEST(QueryRow, TemplatedGetAsTuple) {
  QueryRow row = bigtable_mocks::MakeQueryRow(1, "blah", true);

  using RowType = std::tuple<std::int64_t, std::string, bool>;
  EXPECT_STATUS_OK(row.get<RowType>());
  EXPECT_EQ(std::make_tuple(1, "blah", true), *row.get<RowType>());

  using TooFewTypes = std::tuple<std::int64_t, std::string>;
  EXPECT_THAT(row.get<TooFewTypes>(), Not(IsOk()));
  QueryRow copy = row;
  EXPECT_THAT(std::move(copy).get<TooFewTypes>(), Not(IsOk()));

  using TooManyTypes = std::tuple<std::int64_t, std::string, bool, bool>;
  EXPECT_THAT(row.get<TooManyTypes>(), Not(IsOk()));
  copy = row;
  EXPECT_THAT(std::move(copy).get<TooManyTypes>(), Not(IsOk()));

  using WrongType = std::tuple<std::int64_t, std::string, std::int64_t>;
  EXPECT_THAT(row.get<WrongType>(), Not(IsOk()));
  copy = row;
  EXPECT_THAT(std::move(copy).get<WrongType>(), Not(IsOk()));

  EXPECT_EQ(std::make_tuple(1, "blah", true), *std::move(row).get<RowType>());
}

TEST(MakeQueryRow, ExplicitColumnNames) {
  auto row = bigtable_mocks::MakeQueryRow({{"a", Value(42)}, {"b", Value(52)}});
  EXPECT_EQ(Value(42), *row.get("a"));
  EXPECT_EQ(Value(52), *row.get("b"));
}

TEST(MakeQueryRow, ImplicitColumnNames) {
  auto row = bigtable_mocks::MakeQueryRow(42, 52);
  EXPECT_EQ(Value(42), *row.get("0"));
  EXPECT_EQ(Value(52), *row.get("1"));
}

TEST(QueryRowStreamIterator, Basics) {
  QueryRowStreamIterator end;
  EXPECT_EQ(end, end);

  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow(1, "foo", true));
  rows.emplace_back(bigtable_mocks::MakeQueryRow(2, "bar", true));
  rows.emplace_back(bigtable_mocks::MakeQueryRow(3, "baz", true));

  auto it = QueryRowStreamIterator(MakeQueryRowStreamIteratorSource(rows));
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

TEST(QueryRowStreamIterator, Empty) {
  QueryRowStreamIterator end;
  auto it = QueryRowStreamIterator(MakeQueryRowStreamIteratorSource());
  EXPECT_EQ(it, end);
}

TEST(QueryRowStreamIterator, OneRow) {
  QueryRowStreamIterator end;
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow(1, "foo", true));
  auto it = QueryRowStreamIterator(MakeQueryRowStreamIteratorSource(rows));
  EXPECT_NE(it, end);
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ(rows[0], **it);

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_EQ(it, end);
}

TEST(QueryRowStreamIterator, IterationError) {
  QueryRowStreamIterator end;
  std::vector<StatusOr<QueryRow>> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow(1, "foo", true));
  rows.emplace_back(Status(StatusCode::kUnknown, "some error"));
  rows.emplace_back(bigtable_mocks::MakeQueryRow(2, "bar", true));

  auto it = QueryRowStreamIterator(MakeQueryRowStreamIteratorSource(rows));
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

TEST(QueryRowStreamIterator, ForLoop) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(2)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(3)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(5)}}));

  auto source = MakeQueryRowStreamIteratorSource(rows);
  std::int64_t product = 1;
  for (QueryRowStreamIterator it(source), end; it != end; ++it) {
    ASSERT_STATUS_OK(*it);
    auto num = (*it)->get<std::int64_t>("num");
    ASSERT_STATUS_OK(num);
    product *= *num;
  }
  EXPECT_EQ(product, 30);
}

TEST(QueryRowStreamIterator, RangeForLoopFloat32) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(2.1F)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(3.2F)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(5.4F)}}));

  QueryRowRange range(MakeQueryRowStreamIteratorSource(rows));
  float sum = 0;
  for (auto const& row : range) {
    ASSERT_STATUS_OK(row);
    auto num = row->get<float>("num");
    ASSERT_STATUS_OK(num);
    sum += *num;
  }
  EXPECT_FLOAT_EQ(sum, 10.7F);
}

TEST(QueryRowStreamIterator, RangeForLoop) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(2)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(3)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(5)}}));

  QueryRowRange range(MakeQueryRowStreamIteratorSource(rows));
  std::int64_t product = 1;
  for (auto const& row : range) {
    ASSERT_STATUS_OK(row);
    auto num = row->get<std::int64_t>("num");
    ASSERT_STATUS_OK(num);
    product *= *num;
  }
  EXPECT_EQ(product, 30);
}

TEST(QueryRowStreamIterator, MovedFromValueOk) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(1)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(2)}}));

  QueryRowRange range(MakeQueryRowStreamIteratorSource(rows));
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

  auto it = TupleIterator(
      QueryRowStreamIterator(MakeQueryRowStreamIteratorSource(rows)),
      QueryRowStreamIterator());

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

  auto it =
      TupleIterator(QueryRowStreamIterator(MakeQueryRowStreamIteratorSource()),
                    QueryRowStreamIterator());
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

  auto it = TupleIterator(
      QueryRowStreamIterator(MakeQueryRowStreamIteratorSource(rows)),
      QueryRowStreamIterator());

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

  QueryRowRange range(MakeQueryRowStreamIteratorSource(rows));
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
  QueryRowRange range(MakeQueryRowStreamIteratorSource(rows));
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

  QueryRowRange range(MakeQueryRowStreamIteratorSource(rows));
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

  QueryRowRange range(MakeQueryRowStreamIteratorSource(rows));

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
  QueryRowRange range(MakeQueryRowStreamIteratorSource(rows));
  auto row = GetSingularRow(range);
  EXPECT_THAT(row,
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("no rows")));
}

TEST(GetSingularRow, TupleStreamEmpty) {
  std::vector<QueryRow> rows;
  QueryRowRange range(MakeQueryRowStreamIteratorSource(rows));
  auto row = GetSingularRow(StreamOf<std::tuple<std::int64_t>>(range));
  EXPECT_THAT(row,
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("no rows")));
}

TEST(GetSingularRow, BasicSingleRow) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(1)}}));

  QueryRowRange range(MakeQueryRowStreamIteratorSource(rows));
  auto row = GetSingularRow(range);
  ASSERT_STATUS_OK(row);
  EXPECT_EQ(1, *row->get<std::int64_t>(0));
}

TEST(GetSingularRow, TupleStreamSingleRow) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(1)}}));

  auto row_range = QueryRowRange(MakeQueryRowStreamIteratorSource(rows));
  auto tup_range = StreamOf<std::tuple<std::int64_t>>(row_range);

  auto row = GetSingularRow(tup_range);
  ASSERT_STATUS_OK(row);
  EXPECT_EQ(1, std::get<0>(*row));
}

TEST(GetSingularRow, BasicTooManyRows) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(1)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(2)}}));

  QueryRowRange range(MakeQueryRowStreamIteratorSource(rows));
  auto row = GetSingularRow(range);
  EXPECT_THAT(
      row, StatusIs(StatusCode::kInvalidArgument, HasSubstr("too many rows")));
}

TEST(GetSingularRow, TupleStreamTooManyRows) {
  std::vector<QueryRow> rows;
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(1)}}));
  rows.emplace_back(bigtable_mocks::MakeQueryRow({{"num", Value(2)}}));

  QueryRowRange range(MakeQueryRowStreamIteratorSource(rows));
  auto row = GetSingularRow(StreamOf<std::tuple<std::int64_t>>(range));
  EXPECT_THAT(
      row, StatusIs(StatusCode::kInvalidArgument, HasSubstr("too many rows")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
