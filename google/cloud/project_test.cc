// Copyright 2021 Google LLC
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

#include "google/cloud/project.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Eq;

TEST(Project, Basics) {
  Project p("p1");
  EXPECT_EQ("p1", p.project_id());
  EXPECT_EQ("projects/p1", p.FullName());

  auto copy = p;
  EXPECT_EQ(copy, p);
  EXPECT_EQ("p1", copy.project_id());
  EXPECT_EQ("projects/p1", copy.FullName());

  auto moved = std::move(copy);
  EXPECT_EQ(moved, p);
  EXPECT_EQ("p1", moved.project_id());
  EXPECT_EQ("projects/p1", moved.FullName());

  Project p2("p2");
  EXPECT_NE(p2, p);
  EXPECT_EQ("p2", p2.project_id());
  EXPECT_EQ("projects/p2", p2.FullName());
}

TEST(Project, OutputStream) {
  Project p("p1");
  std::ostringstream os;
  os << p;
  EXPECT_EQ("projects/p1", os.str());
}

TEST(Project, MakeProject) {
  auto const k_valid_project_name = Project("p1").FullName();
  auto p = MakeProject(k_valid_project_name);
  ASSERT_THAT(p, IsOk());
  EXPECT_THAT(p->FullName(), Eq(k_valid_project_name));

  for (std::string invalid : {
           "",
           "projects/",
           "project/p1",
           "projects//p1",
           "/projects/p1",
           "projects/p1/",
           "projects/p1/etc",
       }) {
    auto p = MakeProject(invalid);
    EXPECT_THAT(p, StatusIs(StatusCode::kInvalidArgument,
                            "Improperly formatted Project: " + invalid));
  }
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
