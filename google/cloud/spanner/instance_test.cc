// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/instance.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;

TEST(Instance, Basics) {
  Instance in("p1", "i1");
  EXPECT_EQ("p1", in.project_id());
  EXPECT_EQ("i1", in.instance_id());
  EXPECT_EQ("projects/p1/instances/i1", in.FullName());

  auto copy = in;
  EXPECT_EQ(copy, in);
  EXPECT_EQ("p1", copy.project_id());
  EXPECT_EQ("i1", copy.instance_id());
  EXPECT_EQ("projects/p1/instances/i1", copy.FullName());

  auto moved = std::move(copy);
  EXPECT_EQ(moved, in);
  EXPECT_EQ("p1", moved.project_id());
  EXPECT_EQ("i1", moved.instance_id());
  EXPECT_EQ("projects/p1/instances/i1", moved.FullName());

  Instance in2("p2", "i2");
  EXPECT_NE(in2, in);
  EXPECT_EQ("p2", in2.project_id());
  EXPECT_EQ("i2", in2.instance_id());
  EXPECT_EQ("projects/p2/instances/i2", in2.FullName());
}

TEST(Instance, OutputStream) {
  Instance in("p1", "i1");
  std::ostringstream os;
  os << in;
  EXPECT_EQ("projects/p1/instances/i1", os.str());
}

TEST(Instance, MakeInstance) {
  auto in = Instance(Project("p1"), "i1");
  EXPECT_EQ(in, MakeInstance(in.FullName()).value());

  for (std::string invalid : {
           "",
           "projects/",
           "projects/p1",
           "projects/p1/instances/",
           "/projects/p1/instances/i1",
           "projects/p1/instances/i1/",
           "projects/p1/instances/i1/etc",
       }) {
    auto in = MakeInstance(invalid);
    EXPECT_THAT(in, StatusIs(StatusCode::kInvalidArgument,
                             "Improperly formatted Instance: " + invalid));
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
