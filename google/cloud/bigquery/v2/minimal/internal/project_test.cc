// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigquery/v2/minimal/testing/project_test_utils.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TEST(ProjectTest, ProjectToJson) {
  auto const text = bigquery_v2_minimal_testing::MakeProjectJsonText();
  auto expected_json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(expected_json.is_object());

  auto input = bigquery_v2_minimal_testing::MakeProject();

  nlohmann::json actual_json;
  to_json(actual_json, input);

  EXPECT_EQ(expected_json.dump(), actual_json.dump());
}

TEST(ProjectTest, ProjectFromJson) {
  auto const text = bigquery_v2_minimal_testing::MakeProjectJsonText();
  auto json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(json.is_object());

  auto expected = bigquery_v2_minimal_testing::MakeProject();

  Project actual;
  from_json(json, actual);

  bigquery_v2_minimal_testing::AssertEquals(expected, actual);
}

TEST(ProjectTest, ProjectDebugString) {
  auto project = bigquery_v2_minimal_testing::MakeProject();

  EXPECT_EQ(project.DebugString("Project", TracingOptions{}),
            R"(Project {)"
            R"( kind: "p-kind")"
            R"( id: "p-id")"
            R"( friendly_name: "p-friendly-name")"
            R"( project_reference {)"
            R"( project_id: "p-project-id")"
            R"( })"
            R"( numeric_id: 123)"
            R"( })");

  EXPECT_EQ(project.DebugString("Project",
                                TracingOptions{}.SetOptions(
                                    "truncate_string_field_longer_than=7")),
            R"(Project {)"
            R"( kind: "p-kind")"
            R"( id: "p-id")"
            R"( friendly_name: "p-frien...<truncated>...")"
            R"( project_reference {)"
            R"( project_id: "p-proje...<truncated>...")"
            R"( })"
            R"( numeric_id: 123)"
            R"( })");

  EXPECT_EQ(project.DebugString(
                "Project", TracingOptions{}.SetOptions("single_line_mode=F")),
            R"(Project {
  kind: "p-kind"
  id: "p-id"
  friendly_name: "p-friendly-name"
  project_reference {
    project_id: "p-project-id"
  }
  numeric_id: 123
})");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
