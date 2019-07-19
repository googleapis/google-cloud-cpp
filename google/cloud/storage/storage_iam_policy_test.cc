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

#include "google/cloud/storage/iam_policy.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>
#include <type_traits>

namespace google {
namespace cloud {
namespace storage {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {

static_assert(std::is_copy_constructible<NativeExpression>::value,
              "NativeExpression shoud be copy constructible");
static_assert(std::is_move_constructible<NativeExpression>::value,
              "NativeExpression shoud be move constructible");
static_assert(std::is_copy_assignable<NativeExpression>::value,
              "NativeExpression shoud be copy assignable");
static_assert(std::is_move_assignable<NativeExpression>::value,
              "NativeExpression shoud be move assignable");

static_assert(std::is_copy_constructible<NativeIamBinding>::value,
              "NativeIamBinding shoud be copy constructible");
static_assert(std::is_move_constructible<NativeIamBinding>::value,
              "NativeIamBinding shoud be move constructible");
static_assert(std::is_copy_assignable<NativeIamBinding>::value,
              "NativeIamBinding shoud be copy assignable");
static_assert(std::is_move_assignable<NativeIamBinding>::value,
              "NativeIamBinding shoud be move assignable");

static_assert(std::is_copy_constructible<NativeIamPolicy>::value,
              "NativeIamPolicy shoud be copy constructible");
static_assert(std::is_move_constructible<NativeIamPolicy>::value,
              "NativeIamPolicy shoud be move constructible");
static_assert(std::is_copy_assignable<NativeIamPolicy>::value,
              "NativeIamPolicy shoud be copy assignable");
static_assert(std::is_move_assignable<NativeIamPolicy>::value,
              "NativeIamPolicy shoud be move assignable");

TEST(NativeIamExpression, CtorAndAccessors) {
  NativeExpression expr("expr", "title", "descr", "loc");
  NativeExpression const& const_expr = expr;
  EXPECT_EQ("expr", const_expr.expression());
  EXPECT_EQ("title", const_expr.title());
  EXPECT_EQ("descr", const_expr.description());
  EXPECT_EQ("loc", const_expr.location());

  expr = NativeExpression("expr2");
  EXPECT_EQ("expr2", const_expr.expression());
  EXPECT_EQ("", const_expr.title());
  EXPECT_EQ("", const_expr.description());
  EXPECT_EQ("", const_expr.location());

  expr.set_expression("expr3");
  expr.set_title("title3");
  expr.set_description("descr3");
  expr.set_location("loc3");
  EXPECT_EQ("expr3", const_expr.expression());
  EXPECT_EQ("title3", const_expr.title());
  EXPECT_EQ("descr3", const_expr.description());
  EXPECT_EQ("loc3", const_expr.location());
}

TEST(NativeIamExpression, Printing) {
  NativeExpression expr("expr");
  NativeExpression const& const_expr = expr;

  {
    std::stringstream sstream;
    sstream << const_expr;
    EXPECT_EQ("(expr)", sstream.str());
  }
  expr.set_title("title");
  {
    std::stringstream sstream;
    sstream << const_expr;
    EXPECT_EQ("(expr, title=\"title\")", sstream.str());
  }
  expr.set_description("descr");
  {
    std::stringstream sstream;
    sstream << const_expr;
    EXPECT_EQ("(expr, title=\"title\", description=\"descr\")", sstream.str());
  }
  expr.set_location("loc");
  {
    std::stringstream sstream;
    sstream << const_expr;
    EXPECT_EQ(
        "(expr, title=\"title\", description=\"descr\", location=\"loc\")",
        sstream.str());
  }
}

TEST(NativeIamBinding, CtorAndAccessors) {
  NativeIamBinding binding("role", {"member1", "member2"});
  NativeIamBinding const& const_binding = binding;
  ASSERT_EQ("role", const_binding.role());
  binding.set_role("role2");
  ASSERT_EQ("role2", const_binding.role());
  ASSERT_EQ(std::vector<std::string>({"member1", "member2"}),
            const_binding.members());
  binding.members().emplace_back("member3");
  ASSERT_EQ(std::vector<std::string>({"member1", "member2", "member3"}),
            const_binding.members());
  ASSERT_FALSE(binding.has_condition());

  binding = NativeIamBinding("role", {"member1"}, NativeExpression("expr"));
  ASSERT_EQ("role", const_binding.role());
  ASSERT_EQ(std::vector<std::string>({"member1"}), const_binding.members());
  ASSERT_TRUE(binding.has_condition());
  ASSERT_EQ("expr", binding.condition().expression());

  binding.set_condition(NativeExpression("expr2"));
  ASSERT_TRUE(binding.has_condition());
  ASSERT_EQ("expr2", binding.condition().expression());

  binding.clear_condition();
  ASSERT_FALSE(binding.has_condition());
}

TEST(NativeIamBinding, Printing) {
  NativeIamBinding binding("role", {"member1", "member2"});
  NativeIamBinding const& const_binding = binding;
  {
    std::stringstream sstream;
    sstream << const_binding;
    EXPECT_EQ("role: [member1, member2]", sstream.str());
  }
  binding.set_condition(NativeExpression("expr"));
  {
    std::stringstream sstream;
    sstream << const_binding;
    EXPECT_EQ("role: [member1, member2] when (expr)", sstream.str());
  }
}

TEST(NativeIamPolicy, CtorAndAccessors) {
  NativeIamPolicy policy({NativeIamBinding("role1", {"member1", "member2"})},
                         "etag", 14);
  NativeIamPolicy const& const_policy = policy;
  ASSERT_EQ(14, const_policy.version());
  ASSERT_EQ("etag", const_policy.etag());
  policy.set_version(13);
  ASSERT_EQ(13, const_policy.version());
  policy.set_etag("etag_1");
  ASSERT_EQ("etag_1", const_policy.etag());

  ASSERT_EQ(1U, const_policy.bindings().size());
  ASSERT_EQ("role1", const_policy.bindings()[0].role());
  policy.bindings().emplace_back(
      NativeIamBinding("role2", {"member1", "member3"}));
  ASSERT_EQ(2U, const_policy.bindings().size());
  ASSERT_EQ("role1", const_policy.bindings()[0].role());
  ASSERT_EQ("role2", const_policy.bindings()[1].role());
}

TEST(NativeIamPolicy, Json) {
  NativeIamPolicy policy({NativeIamBinding("role1", {"member1", "member2"})},
                         "etag1", 17);
  auto json = policy.ToJson();
  auto maybe_policy = NativeIamPolicy::CreateFromJson(json);
  ASSERT_STATUS_OK(maybe_policy);
  policy = *std::move(maybe_policy);

  ASSERT_EQ(17, policy.version());
  ASSERT_EQ("etag1", policy.etag());
  ASSERT_EQ(1U, policy.bindings().size());
  ASSERT_EQ("role1", policy.bindings().front().role());
  ASSERT_EQ(std::vector<std::string>({"member1", "member2"}),
            policy.bindings().front().members());
}

// Check that expressions are parsed correctly.
TEST(NativeIamPolicy, ParseExpressionSuccess) {
  auto policy = NativeIamPolicy::CreateFromJson(R"""(
    {
      "bindings": [
        {
          "condition":
            {
              "description": "descr",
              "expression": "expr",
              "location": "loc",
              "title": "title"
            },
          "members": ["member1"],
          "role": "role1"
        }
      ],
      "version": 0
    }
  )""");
  ASSERT_STATUS_OK(policy);
  ASSERT_EQ(1U, policy->bindings().size());
  ASSERT_TRUE(policy->bindings()[0].has_condition());
  EXPECT_EQ("descr", policy->bindings()[0].condition().description());
  EXPECT_EQ("expr", policy->bindings()[0].condition().expression());
  EXPECT_EQ("loc", policy->bindings()[0].condition().location());
  EXPECT_EQ("title", policy->bindings()[0].condition().title());
}

// Check that expressions are parsed correctly when values are not specified.
TEST(NativeIamPolicy, ParseExpressionSuccessDefaults) {
  auto policy = NativeIamPolicy::CreateFromJson(R"""(
    {
      "bindings": [
        {
          "condition": {},
          "members": ["member1"],
          "role": "role1"
        }
      ],
      "version": 0
    }
  )""");
  ASSERT_STATUS_OK(policy);
  ASSERT_EQ(1U, policy->bindings().size());
  ASSERT_TRUE(policy->bindings()[0].has_condition());
  EXPECT_EQ("", policy->bindings()[0].condition().description());
  EXPECT_EQ("", policy->bindings()[0].condition().expression());
  EXPECT_EQ("", policy->bindings()[0].condition().location());
  EXPECT_EQ("", policy->bindings()[0].condition().title());
}

// Check that various errors parsing expressions are caught.
TEST(NativeIamPolicy, ParseConditionFailures) {
  std::string const json_header = R"""(
    {
      "bindings": [
        {
          "condition":
  )""";
  std::string const json_footer = R"""(
          ,
          "members": ["member1"],
          "role": "role1"
        }
      ],
      "version": 0
    }
  )""";
  using testing::HasSubstr;

  auto policy =
      NativeIamPolicy::CreateFromJson(json_header + "0" + json_footer);
  ASSERT_FALSE(policy);
  EXPECT_THAT(policy.status().message(),
              HasSubstr("expected object for 'condition' field."));

  policy = NativeIamPolicy::CreateFromJson(
      json_header + "{\"expression\": {}}" + json_footer);
  ASSERT_FALSE(policy);
  EXPECT_THAT(policy.status().message(),
              HasSubstr("expected string for 'expression' field"));

  policy = NativeIamPolicy::CreateFromJson(
      json_header + "{\"description\": {}}" + json_footer);
  ASSERT_FALSE(policy);
  EXPECT_THAT(policy.status().message(),
              HasSubstr("expected string for 'description' field"));

  policy = NativeIamPolicy::CreateFromJson(json_header + "{\"title\": {}}" +
                                           json_footer);
  ASSERT_FALSE(policy);
  EXPECT_THAT(policy.status().message(),
              HasSubstr("expected string for 'title' field"));

  policy = NativeIamPolicy::CreateFromJson(json_header + "{\"location\": {}}" +
                                           json_footer);
  ASSERT_FALSE(policy);
  EXPECT_THAT(policy.status().message(),
              HasSubstr("expected string for 'location' field"));
}

// Check that bindings are parsed correctly.
TEST(NativeIamPolicy, ParseBindingSuccess) {
  auto policy = NativeIamPolicy::CreateFromJson(R"""(
    {
      "bindings": [
        {
          "members": ["member1", "member2"],
          "role": "role1"
        }
      ]
    }
  )""");
  ASSERT_STATUS_OK(policy);
  ASSERT_EQ(1U, policy->bindings().size());
  auto& binding = policy->bindings()[0];
  auto const& const_binding = binding;
  EXPECT_EQ(binding.members(),
            std::vector<std::string>({"member1", "member2"}));
  EXPECT_EQ(const_binding.members(),
            std::vector<std::string>({"member1", "member2"}));
  EXPECT_EQ("role1", const_binding.role());
}

// Check that bindings are parsed correctly with defaults.
TEST(NativeIamPolicy, ParseBindingSuccessDefaults) {
  auto policy = NativeIamPolicy::CreateFromJson(R"""(
    {
      "bindings": [
        {
        }
      ]
    }
  )""");
  ASSERT_STATUS_OK(policy);
  ASSERT_EQ(1U, policy->bindings().size());
  auto& binding = policy->bindings()[0];
  auto const& const_binding = binding;
  EXPECT_TRUE(binding.members().empty());
  EXPECT_TRUE(const_binding.members().empty());
  EXPECT_EQ("", const_binding.role());
}

// Check that various errors parsing expressions are caught.
TEST(NativeIamPolicy, ParseBindingsFailures) {
  std::string const json_header = R"""(
    {
      "bindings":
  )""";
  std::string const json_footer = R"""(
    }
  )""";
  using testing::HasSubstr;

  auto policy =
      NativeIamPolicy::CreateFromJson(json_header + "0" + json_footer);
  ASSERT_FALSE(policy);
  EXPECT_THAT(policy.status().message(),
              HasSubstr("expected array for 'bindings' field."));

  policy = NativeIamPolicy::CreateFromJson(json_header + "[0]" + json_footer);
  ASSERT_FALSE(policy);
  EXPECT_THAT(policy.status().message(),
              HasSubstr("expected object for 'bindings' entry"));

  policy = NativeIamPolicy::CreateFromJson(json_header + "[{\"role\": 0}]" +
                                           json_footer);
  ASSERT_FALSE(policy);
  EXPECT_THAT(policy.status().message(),
              HasSubstr("expected string for 'role' field"));

  policy = NativeIamPolicy::CreateFromJson(json_header + "[{\"members\": 0}]" +
                                           json_footer);
  ASSERT_FALSE(policy);
  EXPECT_THAT(policy.status().message(),
              HasSubstr("expected array for 'members' field"));

  policy = NativeIamPolicy::CreateFromJson(
      json_header + "[{\"members\": [0]}]" + json_footer);
  ASSERT_FALSE(policy);
  EXPECT_THAT(policy.status().message(),
              HasSubstr("expected string for 'members' entry"));
}

// Check that policies are parsed correctly.
TEST(NativeIamPolicy, ParsePolicySuccess) {
  auto policy = NativeIamPolicy::CreateFromJson(R"""(
    {
      "version": 18,
      "etag": "etag1"
    }
  )""");
  ASSERT_STATUS_OK(policy);
  EXPECT_TRUE(policy->bindings().empty());
  EXPECT_EQ(18, policy->version());
  EXPECT_EQ("etag1", policy->etag());
}

// Check that policies are parsed correctly when defaults are used.
TEST(NativeIamPolicy, ParsePolicySuccessDefaults) {
  auto policy = NativeIamPolicy::CreateFromJson(R"""(
    {
    }
  )""");
  ASSERT_STATUS_OK(policy);
  EXPECT_EQ(0, policy->version());
  EXPECT_EQ("", policy->etag());
}

// Check that various errors parsing policies are caught.
TEST(NativeIamPolicy, ParsePoliciesFailures) {
  using testing::HasSubstr;

  auto policy = NativeIamPolicy::CreateFromJson("{");
  ASSERT_FALSE(policy);
  EXPECT_THAT(policy.status().message(),
              HasSubstr("it failed to parse as valid JSON"));

  policy = NativeIamPolicy::CreateFromJson("0");
  ASSERT_FALSE(policy);
  EXPECT_THAT(policy.status().message(),
              HasSubstr("expected object for top level node"));

  policy = NativeIamPolicy::CreateFromJson("{\"etag\": 0}");
  ASSERT_FALSE(policy);
  EXPECT_THAT(policy.status().message(),
              HasSubstr("expected string for 'etag' field"));

  policy = NativeIamPolicy::CreateFromJson("{\"version\": \"13\"}");
  ASSERT_FALSE(policy);
  EXPECT_THAT(policy.status().message(),
              HasSubstr("expected integer for 'version' field"));
}

TEST(NativeIamPolicy, UnknownFields) {
  auto policy = NativeIamPolicy::CreateFromJson(R"""(
    {
      "bindings": [
        {
          "condition":
            {
              "description": "descr",
              "expression": "expr",
              "location": "loc",
              "title": "title",
              "unknown_expr_field": "opaque1"
            },
          "members": ["member1"],
          "role": "role1",
          "unknown_binding_field": "opaque2"
        }
      ],
      "version": 0,
      "unknown_policy_field": "opaque3"
    }
  )""");
  ASSERT_STATUS_OK(policy);
  auto json = internal::nl::json::parse(policy->ToJson());
  EXPECT_EQ("opaque3", json["unknown_policy_field"]);
  EXPECT_EQ("opaque2", json["bindings"][0]["unknown_binding_field"]);
  EXPECT_EQ("opaque1", json["bindings"][0]["condition"]["unknown_expr_field"]);
}

TEST(NativeIamPolicy, Printing) {
  NativeIamPolicy policy({NativeIamBinding("role", {"member1", "member2"})},
                         "etag1", 18);
  NativeIamPolicy const& const_policy = policy;
  {
    std::stringstream sstream;
    sstream << const_policy;
    EXPECT_EQ(
        "NativeIamPolicy={version=18, bindings=NativeIamBindings={role: "
        "[member1, member2]}, etag=etag1}",
        sstream.str());
  }
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
