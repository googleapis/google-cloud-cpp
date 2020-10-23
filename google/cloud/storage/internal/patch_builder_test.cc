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

#include "google/cloud/storage/internal/patch_builder.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
TEST(PatchBuilderTest, Empty) {
  PatchBuilder builder;
  EXPECT_TRUE(builder.empty());
  EXPECT_EQ("{}", builder.ToString());
}

TEST(PatchBuilderTest, String) {
  PatchBuilder builder;
  builder.AddStringField("set-value", "", "new-value");
  builder.AddStringField("unset-value", "old-value", "");
  builder.AddStringField("untouched-value", "same-value", "same-value");
  nlohmann::json expected{
      {"set-value", "new-value"},
      {"unset-value", nullptr},
  };
  auto actual = nlohmann::json::parse(builder.ToString());
  EXPECT_EQ(expected, actual) << builder.ToString();
}

TEST(PatchBuilderTest, Bool) {
  PatchBuilder builder;
  builder.AddBoolField("set-value", true, false);
  builder.AddBoolField("untouched-value", false, false);
  nlohmann::json expected{
      {"set-value", false},
  };
  auto actual = nlohmann::json::parse(builder.ToString());
  EXPECT_EQ(expected, actual) << builder.ToString();
}

TEST(PatchBuilderTest, Int) {
  PatchBuilder builder;
  builder.AddIntField("set-value", std::int32_t(0), std::int32_t(42));
  builder.AddIntField("unset-value", std::int32_t(42), std::int32_t(0));
  builder.AddIntField("untouched-value", std::int32_t(7), std::int32_t(7));
  nlohmann::json expected{
      {"set-value", 42},
      {"unset-value", nullptr},
  };
  auto actual = nlohmann::json::parse(builder.ToString());
  EXPECT_EQ(expected, actual) << builder.ToString();
}

TEST(PatchBuilderTest, SubPatch) {
  PatchBuilder builder;
  builder.AddStringField("some-field", "", "new-value");
  PatchBuilder subpatch;
  subpatch.AddStringField("set-value", "", "new-value");
  subpatch.AddStringField("unset-value", "old-value", "");
  subpatch.AddStringField("untouched-value", "same-value", "same-value");
  builder.AddSubPatch("the-field", subpatch);
  nlohmann::json expected{
      {"some-field", "new-value"},
      {"the-field", {{"set-value", "new-value"}, {"unset-value", nullptr}}},
  };
  auto actual = nlohmann::json::parse(builder.ToString());
  EXPECT_EQ(expected, actual) << builder.ToString();
}

TEST(PatchBuilderTest, RemoveField) {
  PatchBuilder builder;
  builder.AddStringField("some-field", "", "new-value");
  builder.RemoveField("the-field");
  nlohmann::json expected{
      {"some-field", "new-value"},
      {"the-field", nullptr},
  };
  auto actual = nlohmann::json::parse(builder.ToString());
  EXPECT_EQ(expected, actual) << builder.ToString();
}

TEST(PatchBuilderTest, SetStringField) {
  PatchBuilder builder;
  builder.SetStringField("some-field", "new-value");
  builder.SetStringField("empty-field", "");
  nlohmann::json expected{
      {"some-field", "new-value"},
      {"empty-field", ""},
  };
  auto actual = nlohmann::json::parse(builder.ToString());
  EXPECT_EQ(expected, actual) << builder.ToString();
}

TEST(PatchBuilderTest, SetBoolField) {
  PatchBuilder builder;
  builder.SetBoolField("true-field", true);
  builder.SetBoolField("false-field", false);
  nlohmann::json expected{
      {"true-field", true},
      {"false-field", false},
  };
  auto actual = nlohmann::json::parse(builder.ToString());
  EXPECT_EQ(expected, actual) << builder.ToString();
}

TEST(PatchBuilderTest, SetIntField) {
  PatchBuilder builder;
  builder.SetIntField("field-32-7", std::int32_t(7));
  builder.SetIntField("field-32-0", std::int32_t(0));
  builder.SetIntField("field-u32-7", std::uint32_t(7));
  builder.SetIntField("field-u32-0", std::uint32_t(0));
  builder.SetIntField("field-64-7", std::int64_t(7));
  builder.SetIntField("field-64-0", std::int64_t(0));
  builder.SetIntField("field-u64-7", std::uint64_t(7));
  builder.SetIntField("field-u64-0", std::uint64_t(0));
  nlohmann::json expected{
      {"field-32-7", std::int32_t(7)},   {"field-32-0", std::int32_t(0)},
      {"field-u32-7", std::uint32_t(7)}, {"field-u32-0", std::uint32_t(0)},
      {"field-64-7", std::int64_t(7)},   {"field-64-0", std::int64_t(0)},
      {"field-u64-7", std::uint64_t(7)}, {"field-u64-0", std::uint64_t(0)},
  };
  auto actual = nlohmann::json::parse(builder.ToString());
  EXPECT_EQ(expected, actual) << builder.ToString();
}

TEST(PatchBuilderTest, SetArrayField) {
  PatchBuilder builder;
  builder.SetArrayField("field-a", nlohmann::json::array().dump());
  builder.SetArrayField("field-b",
                        nlohmann::json::array({"foo", "bar"}).dump());
  builder.SetArrayField("field-c", nlohmann::json::array({2, 3, 5, 7}).dump());
  builder.SetArrayField("field-d",
                        nlohmann::json::array({false, true, true}).dump());

  nlohmann::json expected{
      {"field-a", std::vector<std::string>{}},
      {"field-b", std::vector<std::string>{"foo", "bar"}},
      {"field-c", std::vector<int>{2, 3, 5, 7}},
      {"field-d", std::vector<bool>{false, true, true}},
      {"field-a", {{"entity", "entity-0"}, {"role", "role-0"}}},
  };
  auto actual = nlohmann::json::parse(builder.ToString());
  EXPECT_EQ(expected, actual) << builder.ToString();
}
}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
