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
#include <gmock/gmock.h>

using namespace google::cloud;

TEST(LogSeverityTest, Streaming) {
  std::ostringstream os;
  os << Severity::TRACE;
  EXPECT_EQ("TRACE", os.str());
}

TEST(LogSinkTest, CompileTimeEnabled) {
  EXPECT_TRUE(LogSink::CompileTimeEnabled(Severity::CRITICAL));
  if (Severity::LOWEST_ENABLED >= Severity::TRACE) {
    EXPECT_FALSE(LogSink::CompileTimeEnabled(Severity::TRACE));
  }
}

TEST(LogSinkTest, RuntimeSeverity) {
  LogSink sink;
  EXPECT_EQ(Severity::LOWEST_ENABLED, sink.minimum_severity());
  sink.set_minimum_severity(Severity::ERROR);
  EXPECT_EQ(Severity::ERROR, sink.minimum_severity());
}

namespace {
class MockLogBackend : public LogBackend {
 public:
  MOCK_METHOD1(Process, void(LogRecord const&));
};
} // namespace

TEST(LogSinkTest, BackendAddRemove) {
  LogSink sink;
  EXPECT_TRUE(sink.empty());
  long id = sink.AddBackend(std::make_shared<MockLogBackend>());
  EXPECT_FALSE(sink.empty());
  sink.RemoveBackend(id);
  EXPECT_TRUE(sink.empty());
}

TEST(LogSinkTest, ClearBackend) {
  LogSink sink;  
  (void)sink.AddBackend(std::make_shared<MockLogBackend>());
  (void)sink.AddBackend(std::make_shared<MockLogBackend>());
  EXPECT_FALSE(sink.empty());
  sink.ClearBackends();
  EXPECT_TRUE(sink.empty());
}