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

#include "google/cloud/bigtable/iam_binding.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <fstream>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {

TEST(IamBinding, IterCtor) {
  std::vector<std::string> expected({"mem1", "mem2", "mem3", "mem1"});
  auto binding = IamBinding("role", expected.begin(), expected.end());
  EXPECT_EQ("role", binding.role());
  EXPECT_EQ(expected, std::vector<std::string>(binding.members().begin(),
                                               binding.members().end()));
}

TEST(IamBinding, IniListCtor) {
  auto binding = IamBinding("role", {"mem1", "mem2", "mem3", "mem1"});
  EXPECT_EQ("role", binding.role());
  std::vector<std::string> expected({"mem1", "mem2", "mem3", "mem1"});
  EXPECT_EQ(expected, std::vector<std::string>(binding.members().begin(),
                                               binding.members().end()));
}

TEST(IamBinding, VectorCtor) {
  std::vector<std::string> expected({"mem1", "mem2", "mem3", "mem1"});
  auto binding = IamBinding("role", expected);
  EXPECT_EQ("role", binding.role());
  EXPECT_EQ(expected, std::vector<std::string>(binding.members().begin(),
                                               binding.members().end()));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
