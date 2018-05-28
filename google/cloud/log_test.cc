// Copyright 2018 Google LLC
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

#include "google/cloud/log.h"
#include <gtest/gtest.h>

using namespace google::cloud;

TEST(LogTest, Streaming) {
  std::ostringstream os;
  os << Severity::TRACE;
  EXPECT_EQ("TRACE", os.str());
}

TEST(LogTest, CompileTimeEnabled) {
  EXPECT_TRUE(Log::CompileTimeEnabled(Severity::CRITICAL));
  if (Severity::LOWEST_ENABLED >= Severity::TRACE) {
    EXPECT_FALSE(Log::CompileTimeEnabled(Severity::TRACE));
  }
}
