// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/mutations.h"
#include "google/cloud/spanner/bytes.h"
#include "google/cloud/spanner/date.h"
#include "google/cloud/spanner/json.h"
#include "google/cloud/spanner/keys.h"
#include "google/cloud/spanner/numeric.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "absl/types/optional.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsProtoApproximatelyEqual;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::protobuf::TextFormat;
using ::testing::HasSubstr;

TEST(MutationsTest, Default) {
  Mutation actual;
  EXPECT_EQ(actual, actual);
}

TEST(MutationsTest, PrintTo) {
  Mutation insert =
      MakeInsertMutation("table-name", {}, std::string("test-string"));
  std::ostringstream os;
  PrintTo(insert, &os);
  auto actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("test-string"));
  EXPECT_THAT(actual, HasSubstr("Mutation={"));
}

TEST(MutationsTest, InsertSimple) {
  Mutation empty;
  Mutation insert =
      MakeInsertMutation("table-name", {"col_a", "col_b", "col_c"},
                         std::string("foo"), std::string("bar"), true);
  EXPECT_EQ(insert, insert);
  EXPECT_NE(insert, empty);

  auto actual = std::move(insert).as_proto();
  auto constexpr kText = R"pb(
    insert: {
      columns: "col_a"
      columns: "col_b"
      columns: "col_c"
      table: "table-name"
      values: {
        values: { string_value: "foo" }
        values: { string_value: "bar" }
        values: { bool_value: true }
      }
    }
  )pb";
  google::spanner::v1::Mutation expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(MutationsTest, InsertFloat32) {
  auto builder = InsertMutationBuilder("table-name", {"col1", "col2"})
                     .EmplaceRow(1, 3.14F);
  Mutation insert = builder.Build();
  Mutation moved = std::move(builder).Build();
  EXPECT_EQ(insert, moved);

  auto actual = std::move(insert).as_proto();
  auto constexpr kText = R"pb(
    insert: {
      table: "table-name"
      columns: "col1"
      columns: "col2"
      values {
        values { string_value: "1" }
        values { number_value: 3.14 }
      }
    }
  )pb";
  google::spanner::v1::Mutation expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));

  // Compare number_value using the (larger) float epsilon.
  EXPECT_THAT(actual, IsProtoApproximatelyEqual(
                          expected, std::numeric_limits<float>::epsilon(),
                          std::numeric_limits<float>::epsilon()));
}

TEST(MutationsTest, InsertComplex) {
  auto builder = InsertMutationBuilder("table-name", {"col1", "col2", "col3"})
                     .AddRow({Value(42), Value("foo"), Value(false)})
                     .EmplaceRow(absl::optional<std::int64_t>(), "bar",
                                 absl::optional<bool>{});
  Mutation insert = builder.Build();
  Mutation moved = std::move(builder).Build();
  EXPECT_EQ(insert, moved);

  auto actual = std::move(insert).as_proto();
  auto constexpr kText = R"pb(
    insert: {
      table: "table-name"
      columns: "col1"
      columns: "col2"
      columns: "col3"
      values: {
        values: { string_value: "42" }
        values: { string_value: "foo" }
        values: { bool_value: false }
      }
      values: {
        values: { null_value: NULL_VALUE }
        values: { string_value: "bar" }
        values: { null_value: NULL_VALUE }
      }
    }
  )pb";
  google::spanner::v1::Mutation expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(MutationsTest, UpdateSimple) {
  Mutation empty;
  Mutation update =
      MakeUpdateMutation("table-name", {"col_a", "col_b", "col_c"},
                         std::string("foo"), std::string("bar"), true);
  EXPECT_EQ(update, update);
  EXPECT_NE(update, empty);

  auto actual = std::move(update).as_proto();
  auto constexpr kText = R"pb(
    update: {
      table: "table-name"
      columns: "col_a"
      columns: "col_b"
      columns: "col_c"
      values: {
        values: { string_value: "foo" }
        values: { string_value: "bar" }
        values: { bool_value: true }
      }
    }
  )pb";
  google::spanner::v1::Mutation expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(MutationsTest, UpdateComplex) {
  auto builder = UpdateMutationBuilder("table-name", {"col_a", "col_b"})
                     .AddRow({Value(std::vector<std::string>{}), Value(7.0)})
                     .EmplaceRow(std::vector<std::string>{"a", "b"},
                                 absl::optional<double>{});
  Mutation update = builder.Build();
  Mutation moved = std::move(builder).Build();
  EXPECT_EQ(update, moved);

  auto actual = std::move(update).as_proto();
  auto constexpr kText = R"pb(
    update: {
      table: "table-name"
      columns: "col_a"
      columns: "col_b"
      values: {
        values: { list_value: {} }
        values: { number_value: 7.0 }
      }
      values: {
        values: {
          list_value: {
            values: { string_value: "a" }
            values: { string_value: "b" }
          }
        }
        values: { null_value: NULL_VALUE }
      }
    }
  )pb";
  google::spanner::v1::Mutation expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(MutationsTest, InsertOrUpdateSimple) {
  Mutation empty;
  Mutation update =
      MakeInsertOrUpdateMutation("table-name", {"col_a", "col_b", "col_c"},
                                 std::string("foo"), std::string("bar"), true);
  EXPECT_EQ(update, update);
  EXPECT_NE(update, empty);

  auto actual = std::move(update).as_proto();
  auto constexpr kText = R"pb(
    insert_or_update: {
      table: "table-name"
      columns: "col_a"
      columns: "col_b"
      columns: "col_c"
      values: {
        values: { string_value: "foo" }
        values: { string_value: "bar" }
        values: { bool_value: true }
      }
    }
  )pb";
  google::spanner::v1::Mutation expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(MutationsTest, InsertOrUpdateComplex) {
  auto builder = InsertOrUpdateMutationBuilder("table-name", {"col_a", "col_b"})
                     .AddRow({Value(std::make_tuple("a", 7.0))})
                     .EmplaceRow(std::make_tuple("b", 8.0));
  Mutation update = builder.Build();
  Mutation moved = std::move(builder).Build();
  EXPECT_EQ(update, moved);

  auto actual = std::move(update).as_proto();
  auto constexpr kText = R"pb(
    insert_or_update: {
      table: "table-name"
      columns: "col_a"
      columns: "col_b"
      values: {
        values: {
          list_value: {
            values: { string_value: "a" }
            values: { number_value: 7.0 }
          }
        }
      }
      values: {
        values: {
          list_value: {
            values: { string_value: "b" }
            values: { number_value: 8.0 }
          }
        }
      }
    }
  )pb";
  google::spanner::v1::Mutation expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(MutationsTest, ReplaceSimple) {
  Mutation empty;
  Mutation replace =
      MakeReplaceMutation("table-name", {"col_a", "col_b", "col_c"},
                          std::string("foo"), std::string("bar"), true);
  EXPECT_EQ(replace, replace);
  EXPECT_NE(replace, empty);

  auto actual = std::move(replace).as_proto();
  auto constexpr kText = R"pb(
    replace: {
      table: "table-name"
      columns: "col_a"
      columns: "col_b"
      columns: "col_c"
      values: {
        values: { string_value: "foo" }
        values: { string_value: "bar" }
        values: { bool_value: true }
      }
    }
  )pb";
  google::spanner::v1::Mutation expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(MutationsTest, ReplaceComplex) {
  auto builder = ReplaceMutationBuilder("table-name", {"col_a", "col_b"})
                     .EmplaceRow("a", 7.0)
                     .AddRow({Value("b"), Value(8.0)});
  Mutation update = builder.Build();
  Mutation moved = std::move(builder).Build();
  EXPECT_EQ(update, moved);

  auto actual = std::move(update).as_proto();
  auto constexpr kText = R"pb(
    replace: {
      table: "table-name"
      columns: "col_a"
      columns: "col_b"
      values: {
        values: { string_value: "a" }
        values: { number_value: 7.0 }
      }
      values: {
        values: { string_value: "b" }
        values: { number_value: 8.0 }
      }
    }
  )pb";
  google::spanner::v1::Mutation expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(MutationsTest, DeleteSimple) {
  auto ks = KeySet().AddKey(MakeKey("key-to-delete"));
  Mutation dele = MakeDeleteMutation("table-name", std::move(ks));
  EXPECT_EQ(dele, dele);

  Mutation empty;
  EXPECT_NE(dele, empty);

  auto actual = std::move(dele).as_proto();
  auto constexpr kText = R"pb(
    delete: {
      table: "table-name"
      key_set: { keys: { values { string_value: "key-to-delete" } } }
    }
  )pb";
  google::spanner::v1::Mutation expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(MutationsTest, SpannerTypes) {
  Mutation empty;
  auto bytes = Bytes("bytes");
  auto date = Date(2022, 3, 30);
  auto json = Json("{true}");
  auto numeric = MakeNumeric(42).value();
  auto pg_numeric = MakePgNumeric(131072).value();
  auto timestamp = Timestamp();
  Mutation insert = MakeInsertMutation(
      "table-name",
      {"bytes", "date", "json", "numeric", "pg_numeric", "timestamp"},  //
      bytes, date, json, numeric, pg_numeric, timestamp);
  EXPECT_EQ(insert, insert);
  EXPECT_NE(insert, empty);

  auto actual = std::move(insert).as_proto();
  auto constexpr kText = R"pb(
    insert {
      table: "table-name"
      columns: "bytes"
      columns: "date"
      columns: "json"
      columns: "numeric"
      columns: "pg_numeric"
      columns: "timestamp"
      values {
        values { string_value: "Ynl0ZXMA" }
        values { string_value: "2022-03-30" }
        values { string_value: "{true}" }
        values { string_value: "42" }
        values { string_value: "131072" }
        values { string_value: "1970-01-01T00:00:00Z" }
      }
    }
  )pb";
  google::spanner::v1::Mutation expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(MutationsTest, FluentInsertBuilder) {
  static_assert(
      std::is_rvalue_reference<decltype(std::declval<InsertMutationBuilder>()
                                            .EmplaceRow()
                                            .AddRow({})
                                            .Build())>::value,
      "Build() should return an rvalue if called fluently on a temporary");

  std::string const data(128, 'x');
  std::string blob = data;
  Mutation m = InsertMutationBuilder("table-name", {"col_a"})
                   .EmplaceRow(std::move(blob))
                   .AddRow({Value(data)})
                   .Build();
  auto actual = std::move(m).as_proto();
  EXPECT_EQ(2, actual.insert().values().size());
  EXPECT_EQ(data, actual.insert().values(0).values(0).string_value());
  EXPECT_EQ(data, actual.insert().values(1).values(0).string_value());
}

TEST(MutationsTest, FluentUpdateBuilder) {
  static_assert(
      std::is_rvalue_reference<decltype(std::declval<UpdateMutationBuilder>()
                                            .EmplaceRow()
                                            .AddRow({})
                                            .Build())>::value,
      "Build() should return an rvalue if called fluently on a temporary");

  std::string const data(128, 'x');
  std::string blob = data;
  Mutation m = UpdateMutationBuilder("table-name", {"col_a"})
                   .EmplaceRow(std::move(blob))
                   .AddRow({Value(data)})
                   .Build();
  auto actual = std::move(m).as_proto();
  EXPECT_EQ(2, actual.update().values().size());
  EXPECT_EQ(data, actual.update().values(0).values(0).string_value());
  EXPECT_EQ(data, actual.update().values(1).values(0).string_value());
}

TEST(MutationsTest, FluentInsertOrUpdateBuilder) {
  static_assert(
      std::is_rvalue_reference<
          decltype(std::declval<InsertOrUpdateMutationBuilder>()
                       .EmplaceRow()
                       .AddRow({})
                       .Build())>::value,
      "Build() should return an rvalue if called fluently on a temporary");

  std::string const data(128, 'x');
  std::string blob = data;
  Mutation m = InsertOrUpdateMutationBuilder("table-name", {"col_a"})
                   .EmplaceRow(std::move(blob))
                   .AddRow({Value(data)})
                   .Build();
  auto actual = std::move(m).as_proto();
  EXPECT_EQ(2, actual.insert_or_update().values().size());
  EXPECT_EQ(data, actual.insert_or_update().values(0).values(0).string_value());
  EXPECT_EQ(data, actual.insert_or_update().values(1).values(0).string_value());
}

TEST(MutationsTest, FluentReplaceBuilder) {
  static_assert(
      std::is_rvalue_reference<decltype(std::declval<ReplaceMutationBuilder>()
                                            .EmplaceRow()
                                            .AddRow({})
                                            .Build())>::value,
      "Build() should return an rvalue if called fluently on a temporary");

  std::string const data(128, 'x');
  std::string blob = data;
  Mutation m = ReplaceMutationBuilder("table-name", {"col_a"})
                   .EmplaceRow(std::move(blob))
                   .AddRow({Value(data)})
                   .Build();
  auto actual = std::move(m).as_proto();
  EXPECT_EQ(2, actual.replace().values().size());
  EXPECT_EQ(data, actual.replace().values(0).values(0).string_value());
  EXPECT_EQ(data, actual.replace().values(1).values(0).string_value());
}

TEST(MutationsTest, FluentDeleteBuilder) {
  static_assert(
      std::is_rvalue_reference<
          decltype(std::declval<DeleteMutationBuilder>().Build())>::value,
      "Build() should return an rvalue if called fluently on a temporary");

  auto ks = KeySet().AddKey(MakeKey("key-to-delete"));
  Mutation m = DeleteMutationBuilder("table-name", ks).Build();
  auto actual = std::move(m).as_proto();
  auto constexpr kText = R"pb(
    delete: {
      table: "table-name"
      key_set: { keys: { values { string_value: "key-to-delete" } } }
    }
  )pb";
  google::spanner::v1::Mutation expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
