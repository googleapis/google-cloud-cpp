// Copyright 2020 Google LLC
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

#include "google/cloud/testing_util/contains_once.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {
namespace {

using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

TEST(ContainsOnce, Match) {
  std::vector<std::string> v1 = {"foo"};
  EXPECT_THAT(v1, ContainsOnce("foo"));
  EXPECT_THAT(v1, ContainsOnce(std::string("foo")));
  EXPECT_THAT(v1, ContainsOnce(Eq("foo")));
  EXPECT_THAT(v1, ContainsOnce(HasSubstr("oo")));
  EXPECT_THAT(v1, ContainsOnce(Not(Eq("bar"))));

  std::vector<std::string> v2 = {"foo", "bar"};
  EXPECT_THAT(v2, ContainsOnce("foo"));
  EXPECT_THAT(v2, ContainsOnce(Eq(std::string("bar"))));
  EXPECT_THAT(v2, ContainsOnce(AnyOf("bar", "baz")));
  EXPECT_THAT(v2, ContainsOnce(HasSubstr("ar")));
  EXPECT_THAT(v2, ContainsOnce(Not(Eq(std::string("foo")))));

  std::vector<std::string> v3 = {"foo", "bar", ""};
  EXPECT_THAT(v3, ContainsOnce(IsEmpty()));
}

TEST(ContainsOnce, NoMatchOrMultipleMatches) {
  std::vector<std::string> v1 = {};
  EXPECT_THAT(v1, Not(ContainsOnce("foo")));
  EXPECT_THAT(v1, Not(ContainsOnce(IsEmpty())));

  std::vector<std::string> v2 = {"foo"};
  EXPECT_THAT(v2, Not(ContainsOnce("bar")));

  std::vector<std::string> v3 = {"foo", "foo"};
  EXPECT_THAT(v3, Not(ContainsOnce("foo")));
  EXPECT_THAT(v3, Not(ContainsOnce("bar")));

  std::vector<std::string> v4 = {"bar", "baz"};
  EXPECT_THAT(v4, Not(ContainsOnce(AnyOf("bar", "baz"))));
  EXPECT_THAT(v4, Not(ContainsOnce(HasSubstr("oo"))));
  EXPECT_THAT(v4, Not(ContainsOnce(HasSubstr("ba"))));
  EXPECT_THAT(v4, Not(ContainsOnce(IsEmpty())));
  EXPECT_THAT(v4, Not(ContainsOnce(Not(IsEmpty()))));
}

}  // namespace
}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
