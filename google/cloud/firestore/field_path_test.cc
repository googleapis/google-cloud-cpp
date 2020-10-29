// Copyright 2018 Google LLC
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

#include "google/cloud/firestore/field_path.h"
#include <gtest/gtest.h>

namespace firestore = google::cloud::firestore;

TEST(FieldPath, EmptyStringInConstructor) {
  const std::vector<std::string> parts = {"a", "", "b"};
  ASSERT_FALSE(firestore::FieldPath(parts).valid());
}

TEST(FieldPath, Component) {
  const std::vector<std::string> parts = {"a..b"};
  auto const path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`a..b`");
  ASSERT_TRUE(path.valid());
}

TEST(FieldPath, Unicode) {
  const std::vector<std::string> parts = {"一", "二", "三"};
  auto const path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`一`.`二`.`三`");
  ASSERT_TRUE(path.valid());
}

TEST(FieldPath, ToApiReprA) {
  const std::vector<std::string> parts = {"a"};
  auto const path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "a");
  ASSERT_TRUE(path.valid());
}

TEST(FieldPath, ToApiReprBacktick) {
  const std::vector<std::string> parts = {"`"};
  auto const path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`\\``");
  ASSERT_TRUE(path.valid());
}

TEST(FieldPath, ToApiReprDot) {
  const std::vector<std::string> parts = {"."};
  auto const path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`.`");
  ASSERT_TRUE(path.valid());
}

TEST(FieldPath, ToApiReprSlash) {
  const std::vector<std::string> parts = {"\\"};
  auto const path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`\\\\`");
  ASSERT_TRUE(path.valid());
}

TEST(FieldPath, ToApiReprDoubleSlash) {
  const std::vector<std::string> parts = {"\\\\"};
  auto const path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`\\\\\\\\`");
  ASSERT_TRUE(path.valid());
}

TEST(FieldPath, ToApiReprUnderscore) {
  const std::vector<std::string> parts = {"_33132"};
  auto const path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "_33132");
  ASSERT_TRUE(path.valid());
}

TEST(FieldPath, ToApiReprUnicodeNonSimple) {
  const std::vector<std::string> parts = {"一"};
  auto const path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`一`");
  ASSERT_TRUE(path.valid());
}

TEST(FieldPath, ToApiReprNumberNonSimple) {
  const std::vector<std::string> parts = {"03"};
  auto const path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`03`");
  ASSERT_TRUE(path.valid());
}

TEST(FieldPath, ToApiReprSimpleWithDot) {
  const std::vector<std::string> parts = {"a.b"};
  auto const path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`a.b`");
  ASSERT_TRUE(path.valid());
}

TEST(FieldPath, ToApiReprNonSimpleWithDot) {
  const std::vector<std::string> parts = {"a.一"};
  auto const path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`a.一`");
  ASSERT_TRUE(path.valid());
}

TEST(FieldPath, ToApiReprSimple) {
  const std::vector<std::string> parts = {"a0332432"};
  auto const path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "a0332432");
  ASSERT_TRUE(path.valid());
}

TEST(FieldPath, ToApiReprChain) {
  const std::vector<std::string> parts = {"a",   "`",    "\\",       "_3", "03",
                                          "a03", "\\\\", "a0332432", "一"};
  auto const path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(),
            "a.`\\``.`\\\\`._3.`03`.a03.`\\\\\\\\`.a0332432.`一`");
  ASSERT_TRUE(path.valid());
}

TEST(FieldPath, FromString) {
  auto const field_path = firestore::FieldPath::FromString("a.b.c");
  ASSERT_EQ(field_path.ToApiRepr(), "a.b.c");
  ASSERT_TRUE(field_path.valid());
}

TEST(FieldPath, FromStringNonSimple) {
  auto const field_path = firestore::FieldPath::FromString("a.一");
  ASSERT_EQ(field_path.ToApiRepr(), "a.`一`");
  ASSERT_TRUE(field_path.valid());
}

TEST(FieldPath, InvalidCharFromString1) {
  ASSERT_FALSE(firestore::FieldPath::FromString("~").valid());
}

TEST(FieldPath, InvalidCharFromString2) {
  ASSERT_FALSE(firestore::FieldPath::FromString("*").valid());
}

TEST(FieldPath, InvalidCharFromString3) {
  ASSERT_FALSE(firestore::FieldPath::FromString("/").valid());
}

TEST(FieldPath, InvalidCharFromString4) {
  ASSERT_FALSE(firestore::FieldPath::FromString("[").valid());
}

TEST(FieldPath, InvalidCharFromString5) {
  ASSERT_FALSE(firestore::FieldPath::FromString("]").valid());
}

TEST(FieldPath, InvalidCharsFromString6) {
  ASSERT_FALSE(firestore::FieldPath::FromString(".").valid());
}

TEST(FieldPath, FromStringEmptyFieldName) {
  ASSERT_FALSE(firestore::FieldPath::FromString("a..b").valid());
}

TEST(FieldPath, Key) {
  std::vector<std::string> const parts = {"a321", "b456"};
  auto const field_path = firestore::FieldPath(parts);
  auto const field_path_same = firestore::FieldPath::FromString("a321.b456");
  std::vector<std::string> const string = {"a321.b456"};
  auto const field_path_different = firestore::FieldPath(string);
  ASSERT_EQ(field_path, field_path_same);
  ASSERT_NE(field_path, field_path_different);
}

TEST(FieldPath, Append) {
  std::vector<std::string> const parts = {"a321", "b456"};
  auto const field_path = firestore::FieldPath(parts);
  auto constexpr kFieldPathString = "c789.d";
  std::vector<std::string> const parts_2 = {"c789", "d"};
  auto const field_path_class = firestore::FieldPath(parts_2);
  auto const string = field_path.Append(kFieldPathString);
  auto const klass = field_path.Append(field_path_class);
  ASSERT_EQ(string.ToApiRepr(), "a321.b456.c789.d");
  ASSERT_EQ(klass.ToApiRepr(), string.ToApiRepr());
}

TEST(FieldPath, AppendInvalid) {
  auto valid_path = firestore::FieldPath::FromString("a.b.c.d");
  auto invalid_path = firestore::FieldPath::FromString("a..b");
  EXPECT_TRUE(valid_path.valid());
  EXPECT_FALSE(invalid_path.valid());
  EXPECT_FALSE(valid_path.Append(invalid_path).valid());
  EXPECT_FALSE(invalid_path.Append(valid_path).valid());
}

TEST(FieldPath, Compare) {
  auto a = firestore::FieldPath::FromString("a.b.c.d");
  auto b = firestore::FieldPath::FromString("b.c.d.e");
  EXPECT_EQ(a, a);
  EXPECT_NE(a, b);
  EXPECT_LT(a, b);
  EXPECT_LE(a, b);
  EXPECT_GT(b, a);
  EXPECT_GE(b, a);
}

TEST(FieldPath, Size) {
  auto const field_path = firestore::FieldPath::FromString("a.b.c");
  ASSERT_TRUE(field_path.valid());
  EXPECT_EQ(3, field_path.size());
}
