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

#include "google/cloud/spanner/mutations.h"
#include "google/cloud/spanner/keys.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "absl/types/optional.h"
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gmock/gmock.h>
#include <cstdint>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

namespace spanner_proto = ::google::spanner::v1;

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
  spanner_proto::Mutation expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(MutationsTest, InsertComplex) {
  Mutation empty;
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
  spanner_proto::Mutation expected;
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
  spanner_proto::Mutation expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(MutationsTest, UpdateComplex) {
  Mutation empty;
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
  spanner_proto::Mutation expected;
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
  spanner_proto::Mutation expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(MutationsTest, InsertOrUpdateComplex) {
  Mutation empty;
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
  spanner_proto::Mutation expected;
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
  spanner_proto::Mutation expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(MutationsTest, ReplaceComplex) {
  Mutation empty;
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
  spanner_proto::Mutation expected;
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
  spanner_proto::Mutation expected;
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
      std::is_rvalue_reference<decltype(
          std::declval<InsertOrUpdateMutationBuilder>()
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
      std::is_rvalue_reference<decltype(
          std::declval<DeleteMutationBuilder>().Build())>::value,
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
  spanner_proto::Mutation expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
