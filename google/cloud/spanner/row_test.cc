// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/row.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <tuple>
#include <utility>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;

// Given a `vector<StatusOr<Row>>` creates a 'Row::Source' object. This is
// helpful for unit testing and letting the test inject a non-OK Status.
RowStreamIterator::Source MakeRowStreamIteratorSource(
    std::vector<StatusOr<Row>> const& rows) {
  std::size_t index = 0;
  return [=]() mutable -> StatusOr<Row> {
    if (index == rows.size()) return Row{};
    return rows[index++];
  };
}

// Given a `vector<Row>` creates a 'Row::Source' object.
RowStreamIterator::Source MakeRowStreamIteratorSource(
    std::vector<Row> const& rows = {}) {
  return MakeRowStreamIteratorSource(
      std::vector<StatusOr<Row>>(rows.begin(), rows.end()));
}

class RowRange {
 public:
  explicit RowRange(RowStreamIterator::Source s) : s_(std::move(s)) {}
  RowStreamIterator begin() { return RowStreamIterator(s_); }
  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  RowStreamIterator end() { return RowStreamIterator(); }

 private:
  RowStreamIterator::Source s_;
};

TEST(Row, DefaultConstruct) {
  Row row;
  EXPECT_EQ(0, row.size());
}

TEST(Row, ValueSemantics) {
  Row row = MakeTestRow(1, "blah", true);

  Row copy = row;
  EXPECT_EQ(copy, row);

  copy = row;
  EXPECT_EQ(copy, row);

  Row move = std::move(row);
  EXPECT_EQ(move, copy);

  row = copy;
  move = std::move(row);
  EXPECT_EQ(move, copy);
}

TEST(Row, BasicAccessors) {
  auto values = std::vector<Value>{Value(1), Value("blah"), Value(true)};
  auto columns = std::vector<std::string>{"a", "b", "c"};
  Row row = MakeTestRow({
      {columns[0], values[0]},  //
      {columns[1], values[1]},  //
      {columns[2], values[2]}   //
  });

  EXPECT_EQ(3, row.size());
  EXPECT_EQ(values, row.values());
  EXPECT_EQ(columns, row.columns());
  EXPECT_EQ(values, std::move(row).values());
}

TEST(Row, GetByPosition) {
  Row row = MakeTestRow(1, "blah", true);

  EXPECT_STATUS_OK(row.get(0));
  EXPECT_STATUS_OK(row.get(1));
  EXPECT_STATUS_OK(row.get(2));
  EXPECT_FALSE(row.get(3).ok());

  EXPECT_EQ(Value(1), *row.get(0));
  EXPECT_EQ(Value("blah"), *row.get(1));
  EXPECT_EQ(Value(true), *row.get(2));
}

TEST(Row, GetByColumnName) {
  Row row = MakeTestRow({
      {"a", Value(1)},       //
      {"b", Value("blah")},  //
      {"c", Value(true)}     //
  });

  EXPECT_STATUS_OK(row.get("a"));
  EXPECT_STATUS_OK(row.get("b"));
  EXPECT_STATUS_OK(row.get("c"));
  EXPECT_FALSE(row.get("not a column name").ok());

  EXPECT_EQ(Value(1), *row.get("a"));
  EXPECT_EQ(Value("blah"), *row.get("b"));
  EXPECT_EQ(Value(true), *row.get("c"));
}

TEST(Row, TemplatedGetByPosition) {
  Row row = MakeTestRow(1, "blah", true);

  EXPECT_STATUS_OK(row.get<std::int64_t>(0));
  EXPECT_STATUS_OK(row.get<std::string>(1));
  EXPECT_STATUS_OK(row.get<bool>(2));

  // Ensures that the wrong type specification results in a failure.
  EXPECT_FALSE(row.get<bool>(0).ok());
  EXPECT_FALSE(row.get<std::int64_t>(1).ok());
  EXPECT_FALSE(row.get<std::string>(2).ok());
  EXPECT_FALSE(row.get<std::int64_t>(3).ok());

  EXPECT_EQ(1, *row.get<std::int64_t>(0));
  EXPECT_EQ("blah", *row.get<std::string>(1));
  EXPECT_EQ(true, *row.get<bool>(2));
}

TEST(Row, TemplatedGetByColumnName) {
  Row row = MakeTestRow({
      {"a", Value(1)},       //
      {"b", Value("blah")},  //
      {"c", Value(true)}     //
  });

  EXPECT_STATUS_OK(row.get<std::int64_t>("a"));
  EXPECT_STATUS_OK(row.get<std::string>("b"));
  EXPECT_STATUS_OK(row.get<bool>("c"));

  // Ensures that the wrong type specification results in a failure.
  EXPECT_FALSE(row.get<bool>("a").ok());
  EXPECT_FALSE(row.get<std::int64_t>("b").ok());
  EXPECT_FALSE(row.get<std::string>("c").ok());
  EXPECT_FALSE(row.get<std::string>("column does not exist").ok());

  EXPECT_EQ(1, *row.get<std::int64_t>("a"));
  EXPECT_EQ("blah", *row.get<std::string>("b"));
  EXPECT_EQ(true, *row.get<bool>("c"));
}

TEST(Row, TemplatedGetAsTuple) {
  Row row = MakeTestRow(1, "blah", true);

  using RowType = std::tuple<std::int64_t, std::string, bool>;
  EXPECT_STATUS_OK(row.get<RowType>());
  EXPECT_EQ(std::make_tuple(1, "blah", true), *row.get<RowType>());

  using TooFewTypes = std::tuple<std::int64_t, std::string>;
  EXPECT_FALSE(row.get<TooFewTypes>().ok());
  Row copy = row;
  EXPECT_FALSE(std::move(copy).get<TooFewTypes>().ok());

  using TooManyTypes = std::tuple<std::int64_t, std::string, bool, bool>;
  EXPECT_FALSE(row.get<TooManyTypes>().ok());
  copy = row;
  EXPECT_FALSE(std::move(copy).get<TooManyTypes>().ok());

  using WrongType = std::tuple<std::int64_t, std::string, std::int64_t>;
  EXPECT_FALSE(row.get<WrongType>().ok());
  copy = row;
  EXPECT_FALSE(std::move(copy).get<WrongType>().ok());

  EXPECT_EQ(std::make_tuple(1, "blah", true), *std::move(row).get<RowType>());
}

TEST(MakeTestRow, ExplicitColumNames) {
  auto row = MakeTestRow({{"a", Value(42)}, {"b", Value(52)}});
  EXPECT_EQ(Value(42), *row.get("a"));
  EXPECT_EQ(Value(52), *row.get("b"));
}

TEST(MakeTestRow, ImplicitColumNames) {
  auto row = MakeTestRow(42, 52);
  EXPECT_EQ(Value(42), *row.get("0"));
  EXPECT_EQ(Value(52), *row.get("1"));
}

TEST(RowStreamIterator, Basics) {
  RowStreamIterator end;
  EXPECT_EQ(end, end);

  std::vector<Row> rows;
  rows.emplace_back(MakeTestRow(1, "foo", true));
  rows.emplace_back(MakeTestRow(2, "bar", true));
  rows.emplace_back(MakeTestRow(3, "baz", true));

  auto it = RowStreamIterator(MakeRowStreamIteratorSource(rows));
  EXPECT_EQ(it, it);
  EXPECT_NE(it, end);
  EXPECT_STATUS_OK(*it);
  EXPECT_EQ(rows[0], **it);

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_NE(it, end);
  EXPECT_STATUS_OK(*it);
  EXPECT_EQ(rows[1], **it);

  it++;
  EXPECT_EQ(it, it);
  EXPECT_NE(it, end);
  EXPECT_STATUS_OK(*it);
  EXPECT_EQ(rows[2], **it);

  // Tests const op*() and op->()
  auto const copy = it;
  EXPECT_EQ(copy, it);
  EXPECT_NE(copy, end);
  EXPECT_STATUS_OK(*copy);
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
  std::vector<Row> rows;
  rows.emplace_back(MakeTestRow(1, "foo", true));
  auto it = RowStreamIterator(MakeRowStreamIteratorSource(rows));
  EXPECT_NE(it, end);
  EXPECT_STATUS_OK(*it);
  EXPECT_EQ(rows[0], **it);

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_EQ(it, end);
}

TEST(RowStreamIterator, IterationError) {
  RowStreamIterator end;
  std::vector<StatusOr<Row>> rows;
  rows.emplace_back(MakeTestRow(1, "foo", true));
  rows.emplace_back(Status(StatusCode::kUnknown, "some error"));
  rows.emplace_back(MakeTestRow(2, "bar", true));

  auto it = RowStreamIterator(MakeRowStreamIteratorSource(rows));
  EXPECT_NE(it, end);
  EXPECT_STATUS_OK(*it);
  EXPECT_EQ(rows[0], *it);

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_NE(it, end);
  EXPECT_FALSE(*it);
  EXPECT_THAT(*it, StatusIs(StatusCode::kUnknown, "some error"));

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_EQ(it, end);
}

TEST(RowStreamIterator, ForLoop) {
  std::vector<Row> rows;
  rows.emplace_back(MakeTestRow({{"num", Value(2)}}));
  rows.emplace_back(MakeTestRow({{"num", Value(3)}}));
  rows.emplace_back(MakeTestRow({{"num", Value(5)}}));

  auto source = MakeRowStreamIteratorSource(rows);
  std::int64_t product = 1;
  for (RowStreamIterator it(source), end; it != end; ++it) {
    EXPECT_STATUS_OK(*it);
    auto num = (*it)->get<std::int64_t>("num");
    EXPECT_STATUS_OK(num);
    product *= *num;
  }
  EXPECT_EQ(product, 30);
}

TEST(RowStreamIterator, RangeForLoop) {
  std::vector<Row> rows;
  rows.emplace_back(MakeTestRow({{"num", Value(2)}}));
  rows.emplace_back(MakeTestRow({{"num", Value(3)}}));
  rows.emplace_back(MakeTestRow({{"num", Value(5)}}));

  RowRange range(MakeRowStreamIteratorSource(rows));
  std::int64_t product = 1;
  for (auto const& row : range) {
    EXPECT_STATUS_OK(row);
    auto num = row->get<std::int64_t>("num");
    EXPECT_STATUS_OK(num);
    product *= *num;
  }
  EXPECT_EQ(product, 30);
}

TEST(TupleStreamIterator, Basics) {
  std::vector<Row> rows;
  rows.emplace_back(MakeTestRow(1, "foo", true));
  rows.emplace_back(MakeTestRow(2, "bar", true));
  rows.emplace_back(MakeTestRow(3, "baz", true));

  using RowType = std::tuple<std::int64_t, std::string, bool>;
  using TupleIterator = TupleStreamIterator<RowType>;

  auto end = TupleIterator();
  EXPECT_EQ(end, end);

  auto it = TupleIterator(RowStreamIterator(MakeRowStreamIteratorSource(rows)),
                          RowStreamIterator());

  EXPECT_EQ(it, it);
  EXPECT_NE(it, end);
  EXPECT_STATUS_OK(*it);
  EXPECT_EQ(std::make_tuple(1, "foo", true), **it);

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_NE(it, end);
  EXPECT_STATUS_OK(*it);
  EXPECT_EQ(std::make_tuple(2, "bar", true), **it);

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_NE(it, end);
  EXPECT_STATUS_OK(*it);
  EXPECT_EQ(std::make_tuple(3, "baz", true), **it);

  // Tests const op*() and op->()
  auto const copy = it;
  EXPECT_EQ(copy, it);
  EXPECT_NE(copy, end);
  EXPECT_STATUS_OK(*copy);
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
  std::vector<Row> rows;
  rows.emplace_back(MakeTestRow(1, "foo", true));
  rows.emplace_back(MakeTestRow(2, "bar", "should be a bool"));
  rows.emplace_back(MakeTestRow(3, "baz", true));

  using RowType = std::tuple<std::int64_t, std::string, bool>;
  using TupleIterator = TupleStreamIterator<RowType>;

  auto end = TupleIterator();
  EXPECT_EQ(end, end);

  auto it = TupleIterator(RowStreamIterator(MakeRowStreamIteratorSource(rows)),
                          RowStreamIterator());

  EXPECT_EQ(it, it);
  EXPECT_NE(it, end);
  EXPECT_STATUS_OK(*it);
  EXPECT_EQ(std::make_tuple(1, "foo", true), **it);

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_NE(it, end);
  EXPECT_FALSE(it->ok());  // Error parsing the 2nd element

  ++it;  // Due to the previous error, jumps straight to "end"
  EXPECT_EQ(it, it);
  EXPECT_EQ(it, end);
}

TEST(TupleStream, Basics) {
  std::vector<Row> rows;
  rows.emplace_back(MakeTestRow(1, "foo", true));
  rows.emplace_back(MakeTestRow(2, "bar", true));
  rows.emplace_back(MakeTestRow(3, "baz", true));

  using RowType = std::tuple<std::int64_t, std::string, bool>;
  RowRange range(MakeRowStreamIteratorSource(rows));
  auto parser = StreamOf<RowType>(range);
  auto it = parser.begin();
  auto end = parser.end();
  EXPECT_EQ(end, end);

  EXPECT_EQ(it, it);
  EXPECT_NE(it, end);
  EXPECT_STATUS_OK(*it);
  EXPECT_EQ(std::make_tuple(1, "foo", true), **it);

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_NE(it, end);
  EXPECT_STATUS_OK(*it);
  EXPECT_EQ(std::make_tuple(2, "bar", true), **it);

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_NE(it, end);
  EXPECT_STATUS_OK(*it);
  EXPECT_EQ(std::make_tuple(3, "baz", true), **it);

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_EQ(it, end);
}

TEST(TupleStream, RangeForLoop) {
  std::vector<Row> rows;
  rows.emplace_back(MakeTestRow({{"num", Value(2)}}));
  rows.emplace_back(MakeTestRow({{"num", Value(3)}}));
  rows.emplace_back(MakeTestRow({{"num", Value(5)}}));
  using RowType = std::tuple<std::int64_t>;

  RowRange range(MakeRowStreamIteratorSource(rows));
  std::int64_t product = 1;
  for (auto const& row : StreamOf<RowType>(range)) {
    EXPECT_STATUS_OK(row);
    product *= std::get<0>(*row);
  }
  EXPECT_EQ(product, 30);
}

TEST(TupleStream, IterationError) {
  std::vector<StatusOr<Row>> rows;
  rows.emplace_back(MakeTestRow(1, "foo", true));
  rows.emplace_back(Status(StatusCode::kUnknown, "some error"));
  rows.emplace_back(MakeTestRow(2, "bar", true));

  RowRange range(MakeRowStreamIteratorSource(rows));

  using RowType = std::tuple<std::int64_t, std::string, bool>;
  auto stream = StreamOf<RowType>(range);

  auto end = stream.end();
  auto it = stream.begin();
  EXPECT_NE(it, end);
  EXPECT_STATUS_OK(*it);
  EXPECT_EQ(std::make_tuple(1, "foo", true), **it);

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_NE(it, end);
  EXPECT_FALSE(*it);
  EXPECT_THAT(*it, StatusIs(StatusCode::kUnknown, "some error"));

  ++it;
  EXPECT_EQ(it, it);
  EXPECT_EQ(it, end);
}

TEST(GetSingularRow, BasicEmpty) {
  std::vector<Row> rows;
  RowRange range(MakeRowStreamIteratorSource(rows));
  auto row = GetSingularRow(range);
  EXPECT_FALSE(row.ok());
  EXPECT_THAT(row,
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("no rows")));
}

TEST(GetSingularRow, TupleStreamEmpty) {
  std::vector<Row> rows;
  RowRange range(MakeRowStreamIteratorSource(rows));
  auto row = GetSingularRow(StreamOf<std::tuple<std::int64_t>>(range));
  EXPECT_FALSE(row.ok());
  EXPECT_THAT(row,
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("no rows")));
}

TEST(GetSingularRow, BasicSingleRow) {
  std::vector<Row> rows;
  rows.emplace_back(MakeTestRow({{"num", Value(1)}}));

  RowRange range(MakeRowStreamIteratorSource(rows));
  auto row = GetSingularRow(range);
  EXPECT_STATUS_OK(row);
  EXPECT_EQ(1, *row->get<std::int64_t>(0));
}

TEST(GetSingularRow, TupleStreamSingleRow) {
  std::vector<Row> rows;
  rows.emplace_back(MakeTestRow({{"num", Value(1)}}));

  auto row_range = RowRange(MakeRowStreamIteratorSource(rows));
  auto tup_range = StreamOf<std::tuple<std::int64_t>>(row_range);

  auto row = GetSingularRow(tup_range);
  EXPECT_STATUS_OK(row);
  EXPECT_EQ(1, std::get<0>(*row));
}

TEST(GetSingularRow, BasicTooManyRows) {
  std::vector<Row> rows;
  rows.emplace_back(MakeTestRow({{"num", Value(1)}}));
  rows.emplace_back(MakeTestRow({{"num", Value(2)}}));

  RowRange range(MakeRowStreamIteratorSource(rows));
  auto row = GetSingularRow(range);
  EXPECT_FALSE(row.ok());
  EXPECT_THAT(
      row, StatusIs(StatusCode::kInvalidArgument, HasSubstr("too many rows")));
}

TEST(GetSingularRow, TupleStreamTooManyRows) {
  std::vector<Row> rows;
  rows.emplace_back(MakeTestRow({{"num", Value(1)}}));
  rows.emplace_back(MakeTestRow({{"num", Value(2)}}));

  RowRange range(MakeRowStreamIteratorSource(rows));
  auto row = GetSingularRow(StreamOf<std::tuple<std::int64_t>>(range));
  EXPECT_THAT(
      row, StatusIs(StatusCode::kInvalidArgument, HasSubstr("too many rows")));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
