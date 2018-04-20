// Copyright 2018 Google Inc.
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

#include "../../firestore/client/field_path.h"
#include <gtest/gtest.h>

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

TEST(FieldPath, EmptyStringInPart) {
  const std::vector<std::string> parts = {"a", "", "b"};
  ASSERT_THROW(auto path = firestore::FieldPath(parts), std::invalid_argument);
}

TEST(FieldPath, InvalidCharsInConstructor) {
  const std::vector<std::string> parts = {"~*/[]"};
  auto path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`~*/[]`");
}

TEST(FieldPath, Component) {
  const std::vector<std::string> parts = {"a..b"};
  auto path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`a..b`");
}

TEST(FieldPath, Unicode) {
  const std::vector<std::string> parts = {"一", "二", "三"};
  auto path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`一`.`二`.`三`");
}

TEST(FieldPath, InvalidChar1) {
  ASSERT_THROW(auto path = firestore::FieldPath::FromString("~"),
               std::invalid_argument);
}

TEST(FieldPath, InvalidChar2) {
  ASSERT_THROW(auto path = firestore::FieldPath::FromString("*"),
               std::invalid_argument);
}

TEST(FieldPath, InvalidChar3) {
  ASSERT_THROW(auto path = firestore::FieldPath::FromString("/"),
               std::invalid_argument);
}

TEST(FieldPath, InvalidChar4) {
  ASSERT_THROW(auto path = firestore::FieldPath::FromString("["),
               std::invalid_argument);
}

TEST(FieldPath, InvalidChar5) {
  ASSERT_THROW(auto path = firestore::FieldPath::FromString("]"),
               std::invalid_argument);
}

TEST(FieldPath, ToApiReprA) {
  const std::vector<std::string> parts = {"a"};
  auto path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "a");
}

TEST(FieldPath, ToApiReprBacktick) {
  const std::vector<std::string> parts = {"`"};
  auto path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`\\``");
}

TEST(FieldPath, ToApiReprDot) {
  const std::vector<std::string> parts = {"."};
  auto path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`.`");
}

TEST(FieldPath, ToApiReprSlash) {
  const std::vector<std::string> parts = {"\\"};
  auto path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`\\\\`");
}

TEST(FieldPath, ToApiReprDoubleSlash) {
  const std::vector<std::string> parts = {"\\\\"};
  auto path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`\\\\\\\\`");
}

TEST(FieldPath, ToApiReprUnderscore) {
  const std::vector<std::string> parts = {"_33132"};
  auto path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "_33132");
}

TEST(FieldPath, ToApiReprUnicodeNonSimple) {
  const std::vector<std::string> parts = {"一"};
  auto path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`一`");
}

TEST(FieldPath, ToApiReprNumberNonSimple) {
  const std::vector<std::string> parts = {"03"};
  auto path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`03`");
}

TEST(FieldPath, ToApiReprSimpleWithDot) {
  const std::vector<std::string> parts = {"a.b"};
  auto path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`a.b`");
}

TEST(FieldPath, ToApiReprNonSimpleWithDot) {
  const std::vector<std::string> parts = {"a.一"};
  auto path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "`a.一`");
}

TEST(FieldPath, ToApiReprSimple) {
  const std::vector<std::string> parts = {"a0332432"};
  auto path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(), "a0332432");
}

TEST(FieldPath, ToApiReprChain) {
  const std::vector<std::string> parts = {"a",   "`",    "\\",       "_3", "03",
                                          "a03", "\\\\", "a0332432", "一"};
  auto path = firestore::FieldPath(parts);
  ASSERT_EQ(path.ToApiRepr(),
            "a.`\\``.`\\\\`._3.`03`.a03.`\\\\\\\\`.a0332432.`一`");
}

TEST(FieldPath, FromString) {
  auto field_path = firestore::FieldPath::FromString("a.b.c");
  ASSERT_EQ(field_path.ToApiRepr(), "a.b.c");
}

TEST(FieldPath, FromStringNonSimple) {
  auto field_path = firestore::FieldPath::FromString("a.一");
  ASSERT_EQ(field_path.ToApiRepr(), "a.`一`");
}

TEST(FieldPath, InvalidCharsFromString1) {
  ASSERT_THROW(auto field_path = firestore::FieldPath::FromString("~"),
               std::invalid_argument);
}

TEST(FieldPath, InvalidCharsFromString2) {
  ASSERT_THROW(auto field_path = firestore::FieldPath::FromString("*"),
               std::invalid_argument);
}

TEST(FieldPath, InvalidCharsFromString3) {
  ASSERT_THROW(auto field_path = firestore::FieldPath::FromString("/"),
               std::invalid_argument);
}

TEST(FieldPath, InvalidCharsFromString4) {
  ASSERT_THROW(auto field_path = firestore::FieldPath::FromString("["),
               std::invalid_argument);
}

TEST(FieldPath, InvalidCharsFromString5) {
  ASSERT_THROW(auto field_path = firestore::FieldPath::FromString("]"),
               std::invalid_argument);
}

TEST(FieldPath, InvalidCharsFromString6) {
  ASSERT_THROW(auto field_path = firestore::FieldPath::FromString("."),
               std::invalid_argument);
}

TEST(FieldPath, FromStringEmptyFieldName) {
  ASSERT_THROW(auto field_path = firestore::FieldPath::FromString("a..b"),
               std::invalid_argument);
}

TEST(FieldPath, Key) {
  std::vector<std::string> parts = {"a321", "b456"};
  auto field_path = firestore::FieldPath(parts);
  auto field_path_same = firestore::FieldPath::FromString("a321.b456");
  std::vector<std::string> string = {"a321.b456"};
  auto field_path_different = firestore::FieldPath(string);
  ASSERT_EQ(field_path, field_path_same);
  ASSERT_NE(field_path, field_path_different);
}

TEST(FieldPath, append) {
  std::vector<std::string> parts = {"a321", "b456"};
  auto field_path = firestore::FieldPath(parts);
  auto field_path_string = "c789.d";
  std::vector<std::string> parts_2 = {"c789", "d"};
  auto field_path_class = firestore::FieldPath(parts_2);
  auto string = field_path.append(field_path_string);
  auto klass = field_path.append(field_path_class);
  ASSERT_EQ(string.ToApiRepr(), "a321.b456.c789.d");
  ASSERT_EQ(klass.ToApiRepr(), string.ToApiRepr());
}
