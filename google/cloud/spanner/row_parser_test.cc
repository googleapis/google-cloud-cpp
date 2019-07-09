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

#include "google/cloud/spanner/row_parser.h"
#include "google/cloud/spanner/value.h"
#include <gmock/gmock.h>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

ValueSource MakeValueSource(std::vector<Value> const& v) {
  std::size_t i = 0;
  return [=]() mutable -> StatusOr<optional<Value>> {
    if (i == v.size()) return optional<Value>{};
    return {v[i++]};
  };
}

template <typename... Ts>
RowParser<Ts...> MakeRowParser(std::vector<Value> const& v) {
  return RowParser<Ts...>(MakeValueSource(v));
}

TEST(RowParser, SuccessEmpty) {
  std::vector<Value> const values = {};
  auto rp = MakeRowParser<std::int64_t>(values);
  auto it = rp.begin();
  auto end = rp.end();
  EXPECT_EQ(it, end);
}

TEST(RowParser, SuccessOneColumn) {
  std::vector<Value> const values = {
      Value(0),  // Row 0
      Value(1),  // Row 1
      Value(2),  // Row 2
      Value(3),  // Row 3
  };
  std::int64_t expected_value = 0;
  for (auto row : MakeRowParser<std::int64_t>(values)) {
    EXPECT_TRUE(row.ok());
    EXPECT_EQ(expected_value, row->get<0>());
    ++expected_value;
  }
}

TEST(RowParser, SuccessTwoColumns) {
  std::vector<Value> const values = {
      Value(true), Value(0),  // Row 0
      Value(true), Value(1),  // Row 1
      Value(true), Value(2),  // Row 2
      Value(true), Value(3),  // Row 3
  };
  std::int64_t expected_value = 0;
  for (auto row : MakeRowParser<bool, std::int64_t>(values)) {
    EXPECT_TRUE(row.ok());
    EXPECT_EQ(true, row->get<0>());
    EXPECT_EQ(expected_value, row->get<1>());
    ++expected_value;
  }
}

TEST(RowParser, FailOneIncompleteRow) {
  std::vector<Value> const values = {
      Value(true)  // Row 0 (incomplete)
  };
  auto rp = MakeRowParser<bool, std::int64_t>(values);
  auto it = rp.begin();
  auto end = rp.end();

  // Row 0
  EXPECT_NE(it, end);
  EXPECT_FALSE(it->ok());
  EXPECT_THAT(it->status().message(), testing::HasSubstr("incomplete row"));
  ++it;

  EXPECT_EQ(it, end);
}

TEST(RowParser, FailOneRow) {
  // 4 rows of bool, std::int64_t
  std::vector<Value> const values = {
      Value(true),  Value(0),             // Row 0
      Value(false), Value(1),             // Row 1
      Value(true),  Value("WRONG TYPE"),  // Row 2
      Value(false), Value(3),             // Row 3
  };
  auto rp = MakeRowParser<bool, std::int64_t>(values);
  auto it = rp.begin();
  auto end = rp.end();

  // Row 0
  EXPECT_NE(it, end);
  EXPECT_TRUE(it->ok());
  EXPECT_EQ(MakeRow(true, 0), **it);
  ++it;

  // Row 1
  EXPECT_NE(it, end);
  EXPECT_TRUE(it->ok());
  EXPECT_EQ(MakeRow(false, 1), **it);
  ++it;

  // Row 2 (this row fails to parse)
  EXPECT_NE(it, end);
  EXPECT_FALSE(it->ok());  // Error
  EXPECT_THAT(it->status().message(), testing::HasSubstr("wrong type"));
  ++it;

  EXPECT_EQ(it, end);  // Done
}

TEST(RowParser, FailAllRows) {
  // 4 rows of bool, std::int64_t
  std::vector<Value> const values = {
      Value(true),  Value(0),  // Row 0
      Value(false), Value(1),  // Row 1
      Value(true),  Value(2),  // Row 2
      Value(false), Value(3),  // Row 3
  };
  auto rp = MakeRowParser<std::string>(values);
  auto it = rp.begin();
  auto end = rp.end();

  EXPECT_NE(it, end);
  EXPECT_FALSE(it->ok());  // Error
  EXPECT_THAT(it->status().message(), testing::HasSubstr("wrong type"));
  ++it;

  EXPECT_EQ(it, end);
}

TEST(RowParser, InputIteratorTraits) {
  // Tests a bunch of static traits about the RowIterator. These *could* be
  // tested in the .h file, but that'd be a bunch of noise in the header.

  std::vector<Value> const values = {Value(true), Value(0)};
  auto rp = MakeRowParser<bool, std::int64_t>(values);

  auto it = rp.begin();
  auto end = rp.end();
  using It = decltype(it);

  static_assert(std::is_same<decltype(it), decltype(end)>::value, "");

  // Member alias: iterator_category
  static_assert(std::is_same<std::input_iterator_tag,
                             typename It::iterator_category>::value,
                "");
  // Member alias: value_type
  static_assert(std::is_same<StatusOr<Row<bool, std::int64_t>>,
                             typename It::value_type>::value,
                "");

  // Member alias: difference_type
  static_assert(
      std::is_same<std::ptrdiff_t, typename It::difference_type>::value, "");

  // Member alias: pointer
  static_assert(
      std::is_same<typename It::value_type*, typename It::pointer>::value, "");

  // Member alias: reference
  static_assert(
      std::is_same<typename It::value_type&, typename It::reference>::value,
      "");

  // it != it2
  static_assert(std::is_same<bool, decltype(it != end)>::value, "");
  static_assert(std::is_same<bool, decltype(!(it == end))>::value, "");

  // *it
  static_assert(std::is_reference<decltype(*it)>::value, "");
  static_assert(std::is_convertible<decltype(*it), It::value_type>::value, "");

  // it->m same as (*it).m
  static_assert(std::is_same<decltype(it->ok()), decltype((*it).ok())>::value,
                "");

  // ++it same as It&
  static_assert(std::is_same<It&, decltype(++it)>::value, "");

  // it++ convertible to value_type
  static_assert(std::is_convertible<decltype(*it++), It::value_type>::value,
                "");
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
