// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/patch_builder.h"
#include "google/cloud/storage/internal/patch_builder_details.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

TEST(PatchBuilderTest, Equality) {
  auto b1 = PatchBuilder{}.AddStringField("field1", "lhs", "rhs");
  auto b2 = PatchBuilder{};
  EXPECT_NE(b1, b2);

  b2 = b1;
  EXPECT_EQ(b1, b2);
}

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
  builder.AddIntField("set-value", static_cast<std::int32_t>(0),
                      static_cast<std::int32_t>(42));
  builder.AddIntField("unset-value", static_cast<std::int32_t>(42),
                      static_cast<std::int32_t>(0));
  builder.AddIntField("untouched-value", static_cast<std::int32_t>(7),
                      static_cast<std::int32_t>(7));
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
  builder.SetIntField("field-32-7", static_cast<std::int32_t>(7));
  builder.SetIntField("field-32-0", static_cast<std::int32_t>(0));
  builder.SetIntField("field-u32-7", static_cast<std::uint32_t>(7));
  builder.SetIntField("field-u32-0", static_cast<std::uint32_t>(0));
  builder.SetIntField("field-64-7", static_cast<std::int64_t>(7));
  builder.SetIntField("field-64-0", static_cast<std::int64_t>(0));
  builder.SetIntField("field-u64-7", static_cast<std::uint64_t>(7));
  builder.SetIntField("field-u64-0", static_cast<std::uint64_t>(0));
  nlohmann::json expected{
      {"field-32-7", static_cast<std::int32_t>(7)},
      {"field-32-0", static_cast<std::int32_t>(0)},
      {"field-u32-7", static_cast<std::uint32_t>(7)},
      {"field-u32-0", static_cast<std::uint32_t>(0)},
      {"field-64-7", static_cast<std::int64_t>(7)},
      {"field-64-0", static_cast<std::int64_t>(0)},
      {"field-u64-7", static_cast<std::uint64_t>(7)},
      {"field-u64-0", static_cast<std::uint64_t>(0)},
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

TEST(PatchBuilderTest, GetPatch) {
  PatchBuilder builder;
  builder.AddStringField("string-field", "", "new-value");
  builder.AddIntField("int-field", 0, 42);
  builder.AddBoolField("bool-field", false, true);
  PatchBuilder subpatch;
  subpatch.AddStringField("set-value", "", "new-value");
  subpatch.AddStringField("unset-value", "old-value", "");
  subpatch.AddStringField("untouched-value", "same-value", "same-value");
  builder.AddSubPatch("the-field", subpatch);
  nlohmann::json expected{
      {"string-field", "new-value"},
      {"int-field", 42},
      {"bool-field", true},
      {"the-field", {{"set-value", "new-value"}, {"unset-value", nullptr}}},
  };
  auto actual = PatchBuilderDetails::GetPatch(builder);
  EXPECT_EQ(expected, actual) << builder.ToString();

  EXPECT_EQ(expected.dump(), builder.ToString());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
