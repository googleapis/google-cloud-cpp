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

#include "google/cloud/spanner/keys.h"
#include <gmock/gmock.h>
#include <cstdint>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

TEST(KeySetTest, NoKeys) {
  KeySet no_keys;
  EXPECT_FALSE(no_keys.IsAll());
}

TEST(KeySetTest, AllKeys) {
  auto all_keys = KeySet::All();
  EXPECT_TRUE(all_keys.IsAll());
}

TEST(KeyRangeTest, ConstructorBoundModeUnspecified) {
  std::string start_value("key0");
  std::string end_value("key1");
  KeyRange<Row<std::string>> closed_range =
      MakeKeyRange(MakeRow(start_value), MakeRow(end_value));

  EXPECT_EQ(start_value, closed_range.start().key().get<0>());
  EXPECT_TRUE(closed_range.start().IsClosed());
  EXPECT_EQ(end_value, closed_range.end().key().get<0>());
  EXPECT_TRUE(closed_range.end().IsClosed());
}

TEST(KeyRangeBoundTest, MakeBoundClosed) {
  std::string key_value("key0");
  auto bound = MakeBoundClosed(MakeRow(key_value));
  EXPECT_EQ(key_value, bound.key().get<0>());
  EXPECT_TRUE(bound.IsClosed());
}

TEST(KeyRangeBoundTest, MakeBoundOpen) {
  std::string key_value_0("key0");
  std::int64_t key_value_1(42);
  auto bound = MakeBoundOpen(MakeRow(key_value_0, key_value_1));
  EXPECT_EQ(key_value_0, bound.key().get<0>());
  EXPECT_EQ(key_value_1, bound.key().get<1>());
  EXPECT_TRUE(bound.IsOpen());
}

TEST(KeyRangeTest, ConstructorClosedClosed) {
  std::string start_value("key0");
  std::string end_value("key1");
  auto start_bound = MakeBoundClosed(MakeRow(start_value));
  auto end_bound = MakeBoundClosed(MakeRow(end_value));
  auto closed_range = MakeKeyRange(start_bound, end_bound);
  EXPECT_EQ(start_value, closed_range.start().key().get<0>());
  EXPECT_TRUE(closed_range.start().IsClosed());
  EXPECT_EQ(end_value, closed_range.end().key().get<0>());
  EXPECT_TRUE(closed_range.end().IsClosed());
}

TEST(KeyRangeTest, ConstructorClosedOpen) {
  std::string start_value("key0");
  std::string end_value("key1");
  auto range = KeyRange<Row<std::string>>(MakeBoundClosed(MakeRow(start_value)),
                                          MakeBoundOpen(MakeRow(end_value)));
  EXPECT_EQ(start_value, range.start().key().get<0>());
  EXPECT_TRUE(range.start().IsClosed());
  EXPECT_EQ(end_value, range.end().key().get<0>());
  EXPECT_TRUE(range.end().IsOpen());
}

TEST(KeyRangeTest, ConstructorOpenClosed) {
  std::string start_value("key0");
  std::string end_value("key1");
  auto range = KeyRange<Row<std::string>>(MakeBoundOpen(MakeRow(start_value)),
                                          MakeBoundClosed(MakeRow(end_value)));
  EXPECT_EQ(start_value, range.start().key().get<0>());
  EXPECT_TRUE(range.start().IsOpen());
  EXPECT_EQ(end_value, range.end().key().get<0>());
  EXPECT_TRUE(range.end().IsClosed());
}

TEST(KeyRangeTest, ConstructorOpenOpen) {
  std::string start_value("key0");
  std::string end_value("key1");
  auto range = KeyRange<Row<std::string>>(MakeBoundOpen(MakeRow(start_value)),
                                          MakeBoundOpen(MakeRow(end_value)));
  EXPECT_EQ(start_value, range.start().key().get<0>());
  EXPECT_TRUE(range.start().IsOpen());
  EXPECT_EQ(end_value, range.end().key().get<0>());
  EXPECT_TRUE(range.end().IsOpen());
}

TEST(KeySetBuilderTest, AllKeys) {
  auto all_keys = KeySetBuilder<Row<std::string, std::string>>::All();
  EXPECT_TRUE(all_keys.IsAll());
}

TEST(KeySetBuilderTest, ConstructorSingleKey) {
  std::string expected_value("key0");
  auto key = MakeRow("key0");
  auto ks = KeySetBuilder<Row<std::string>>(key);
  EXPECT_EQ(expected_value, ks.keys()[0].get<0>());
  EXPECT_FALSE(ks.IsAll());
}

TEST(KeySetBuilderTest, ConstructorKeyRange) {
  std::string start_value("key0");
  std::string end_value("key1");
  auto ks = KeySetBuilder<Row<std::string>>(
      KeyRange<Row<std::string>>(MakeBoundClosed(MakeRow(start_value)),
                                 MakeBoundClosed(MakeRow(end_value))));
  EXPECT_EQ(start_value, ks.key_ranges()[0].start().key().get<0>());
  EXPECT_TRUE(ks.key_ranges()[0].start().IsClosed());
  EXPECT_EQ(end_value, ks.key_ranges()[0].end().key().get<0>());
  EXPECT_TRUE(ks.key_ranges()[0].end().IsClosed());
  EXPECT_FALSE(ks.IsAll());
}

TEST(KeySetBuilderTest, AddKeyToEmptyKeySetBuilder) {
  auto ks = KeySetBuilder<Row<std::int64_t, std::string>>();
  ks.Add(MakeRow(42, "key42"));
  EXPECT_EQ(42, ks.keys()[0].get<0>());
  EXPECT_EQ("key42", ks.keys()[0].get<1>());
  EXPECT_FALSE(ks.IsAll());
}

TEST(KeySetBuilderTest, AddKeyToNonEmptyKeySetBuilder) {
  auto ks = KeySetBuilder<Row<std::int64_t, std::string>>(MakeRow(84, "key84"));
  ks.Add(MakeRow(42, "key42"));
  EXPECT_EQ(84, ks.keys()[0].get<0>());
  EXPECT_EQ("key84", ks.keys()[0].get<1>());
  EXPECT_EQ(42, ks.keys()[1].get<0>());
  EXPECT_EQ("key42", ks.keys()[1].get<1>());
}

TEST(KeySetBuilderTest, AddKeyRangeToEmptyKeySetBuilder) {
  auto ks = KeySetBuilder<Row<std::string, std::string>>();
  auto range = KeyRange<Row<std::string, std::string>>(
      MakeBoundClosed(MakeRow("start00", "start01")),
      MakeBoundClosed(MakeRow("end00", "end01")));
  ks.Add(range);
  EXPECT_EQ("start00", ks.key_ranges()[0].start().key().get<0>());
  EXPECT_EQ("start01", ks.key_ranges()[0].start().key().get<1>());
  EXPECT_EQ("end00", ks.key_ranges()[0].end().key().get<0>());
  EXPECT_EQ("end01", ks.key_ranges()[0].end().key().get<1>());
  EXPECT_TRUE(ks.key_ranges()[0].start().IsClosed());
  EXPECT_TRUE(ks.key_ranges()[0].end().IsClosed());
}

TEST(KeySetBuilderTest, AddKeyRangeToNonEmptyKeySetBuilder) {
  auto ks = KeySetBuilder<Row<std::string, std::string>>(
      KeyRange<Row<std::string, std::string>>(MakeRow("start00", "start01"),
                                              MakeRow("end00", "end01")));
  auto range = KeyRange<Row<std::string, std::string>>(
      MakeBoundOpen(MakeRow("start10", "start11")),
      MakeBoundOpen(MakeRow("end10", "end11")));
  ks.Add(range);
  EXPECT_EQ("start00", ks.key_ranges()[0].start().key().get<0>());
  EXPECT_EQ("start01", ks.key_ranges()[0].start().key().get<1>());
  EXPECT_EQ("end00", ks.key_ranges()[0].end().key().get<0>());
  EXPECT_EQ("end01", ks.key_ranges()[0].end().key().get<1>());
  EXPECT_TRUE(ks.key_ranges()[0].start().IsClosed());
  EXPECT_TRUE(ks.key_ranges()[0].end().IsClosed());
  EXPECT_EQ("start10", ks.key_ranges()[1].start().key().get<0>());
  EXPECT_EQ("start11", ks.key_ranges()[1].start().key().get<1>());
  EXPECT_EQ("end10", ks.key_ranges()[1].end().key().get<0>());
  EXPECT_EQ("end11", ks.key_ranges()[1].end().key().get<1>());
  EXPECT_TRUE(ks.key_ranges()[1].start().IsOpen());
  EXPECT_TRUE(ks.key_ranges()[1].end().IsOpen());
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
