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

#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/setenv.h"
#include <gtest/gtest.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {
namespace {

auto constexpr kVarName = "SCOPED_ENVIRONMENT_TEST";

TEST(ScopedEnvironment, SetOverSet) {
  ScopedEnvironment env_outer(kVarName, "foo");
  EXPECT_EQ(*internal::GetEnv(kVarName), "foo");
  {
    ScopedEnvironment env_inner(kVarName, "bar");
    EXPECT_EQ(*internal::GetEnv(kVarName), "bar");
  }
  EXPECT_EQ(*internal::GetEnv(kVarName), "foo");
}

TEST(ScopedEnvironment, SetOverUnset) {
  ScopedEnvironment env_outer(kVarName, {});
  EXPECT_FALSE(internal::GetEnv(kVarName).has_value());
  {
    ScopedEnvironment env_inner(kVarName, "bar");
    EXPECT_EQ(*internal::GetEnv(kVarName), "bar");
  }
  EXPECT_FALSE(internal::GetEnv(kVarName).has_value());
}

TEST(ScopedEnvironment, UnsetOverSet) {
  ScopedEnvironment env_outer(kVarName, "foo");
  EXPECT_EQ(*internal::GetEnv(kVarName), "foo");
  {
    ScopedEnvironment env_inner(kVarName, {});
    EXPECT_FALSE(internal::GetEnv(kVarName).has_value());
  }
  EXPECT_EQ(*internal::GetEnv(kVarName), "foo");
}

}  // namespace
}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
