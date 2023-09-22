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
namespace bigquery_v2_minimal_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

bigquery_v2_minimal_internal::Project MakeProject() {
  bigquery_v2_minimal_internal::Project expected;
  expected.kind = "p-kind";
  expected.id = "p-id";
  expected.friendly_name = "p-friendly-name";
  expected.numeric_id = 123;
  expected.project_reference.project_id = "p-project-id";

  return expected;
}

void AssertEquals(bigquery_v2_minimal_internal::Project const& lhs,
                  bigquery_v2_minimal_internal::Project const& rhs) {
  EXPECT_EQ(lhs.kind, rhs.kind);
  EXPECT_EQ(lhs.id, rhs.id);
  EXPECT_EQ(lhs.friendly_name, rhs.friendly_name);
  EXPECT_EQ(lhs.numeric_id, rhs.numeric_id);
  EXPECT_EQ(lhs.project_reference.project_id, rhs.project_reference.project_id);
}

std::string MakeProjectJsonText() {
  return R"({"kind":"p-kind")"
         R"(,"id":"p-id")"
         R"(,"friendlyName":"p-friendly-name")"
         R"(,"numericId":"123")"
         R"(,"projectReference":{)"
         R"("projectId":"p-project-id")"
         R"(})"
         R"(})";
}

std::string MakeListProjectsResponseJsonText() {
  auto projects_json_txt = MakeProjectJsonText();
  return R"({"etag": "tag-1",
          "kind": "kind-1",
          "nextPageToken": "npt-123",
          "totalItems": 1,
          "projects": [)" +
         projects_json_txt + R"(]})";
}

std::string MakeListProjectsResponseNoPageTokenJsonText() {
  auto projects_json_txt = MakeProjectJsonText();
  return R"({"etag": "tag-1",
          "kind": "kind-1",
          "totalItems": 1,
          "projects": [)" +
         projects_json_txt + R"(]})";
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_testing
}  // namespace cloud
}  // namespace google
