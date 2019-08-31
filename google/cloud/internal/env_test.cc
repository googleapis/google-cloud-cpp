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

#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/setenv.h"
#include <gmock/gmock.h>

using google::cloud::internal::GetEnv;
using google::cloud::internal::SetEnv;
using google::cloud::internal::UnsetEnv;

/// @test Verify passing an empty string creates an environment variable.
TEST(SetEnv, SetEmptyEnvVar) {
  SetEnv("foo", "");
  EXPECT_TRUE(GetEnv("foo").has_value());
}

/// @test Verify we can unset an environment variable with nullptr.
TEST(SetEnv, UnsetEnvWithNullptr) {
  SetEnv("foo", "");
  EXPECT_TRUE(GetEnv("foo").has_value());
  SetEnv("foo", nullptr);
  EXPECT_FALSE(GetEnv("foo").has_value());
}

/// @test Verify we can unset an environment variable.
TEST(SetEnv, UnsetEnv) {
  SetEnv("foo", "");
  EXPECT_TRUE(GetEnv("foo").has_value());
  UnsetEnv("foo");
  EXPECT_FALSE(GetEnv("foo").has_value());
}
