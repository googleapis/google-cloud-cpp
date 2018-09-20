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
using namespace ::testing;

TEST(LogSeverityTest, Streaming) {
  std::ostringstream os;
  os << Severity::GCP_LS_TRACE;
  EXPECT_EQ("TRACE", os.str());
}

TEST(LogSinkTest, CompileTimeEnabled) {
  EXPECT_TRUE(LogSink::CompileTimeEnabled(Severity::GCP_LS_CRITICAL));
  if (Severity::GCP_LS_LOWEST_ENABLED >= Severity::GCP_LS_TRACE) {
    EXPECT_FALSE(LogSink::CompileTimeEnabled(Severity::GCP_LS_TRACE));
  }
}

TEST(LogSinkTest, RuntimeSeverity) {
  LogSink sink;
  EXPECT_EQ(Severity::GCP_LS_LOWEST_ENABLED, sink.minimum_severity());
  sink.set_minimum_severity(Severity::GCP_LS_ERROR);
  EXPECT_EQ(Severity::GCP_LS_ERROR, sink.minimum_severity());
}

namespace {
class MockLogBackend : public LogBackend {
 public:
  MOCK_METHOD1(Process, void(LogRecord const&));
  MOCK_METHOD1(ProcessWithOwnership, void(LogRecord));
};
}  // namespace

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

TEST(LogSinkTest, LogEnabled) {
  LogSink sink;
  auto backend = std::make_shared<MockLogBackend>();
  EXPECT_CALL(*backend, ProcessWithOwnership(_))
      .WillOnce(Invoke([](LogRecord lr) {
        EXPECT_EQ(Severity::GCP_LS_WARNING, lr.severity);
        EXPECT_EQ("test message", lr.message);
      }));
  sink.AddBackend(backend);

  GOOGLE_CLOUD_CPP_LOG_I(GCP_LS_WARNING, sink) << "test message";
}

TEST(LogSinkTest, LogEnabledMultipleBackends) {
  LogSink sink;
  auto be1 = std::make_shared<MockLogBackend>();
  auto be2 = std::make_shared<MockLogBackend>();
  EXPECT_CALL(*be1, Process(_)).WillOnce(Invoke([](LogRecord const& lr) {
    EXPECT_EQ(Severity::GCP_LS_WARNING, lr.severity);
    EXPECT_EQ("test message", lr.message);
  }));
  sink.AddBackend(be1);
  EXPECT_CALL(*be2, Process(_)).WillOnce(Invoke([](LogRecord const& lr) {
    EXPECT_EQ(Severity::GCP_LS_WARNING, lr.severity);
    EXPECT_EQ("test message", lr.message);
  }));
  sink.AddBackend(be2);

  GOOGLE_CLOUD_CPP_LOG_I(GCP_LS_WARNING, sink) << "test message";
}

TEST(LogSinkTest, LogDefaultInstance) {
  auto backend = std::make_shared<MockLogBackend>();
  EXPECT_CALL(*backend, ProcessWithOwnership(_))
      .WillOnce(Invoke([](LogRecord lr) {
        EXPECT_EQ(Severity::GCP_LS_WARNING, lr.severity);
        EXPECT_EQ("test message", lr.message);
      }));
  LogSink::Instance().AddBackend(backend);

  GCP_LOG(WARNING) << "test message";
  LogSink::Instance().ClearBackends();
}

TEST(LogSinkTest, LogToClog) {
  LogSink::EnableStdClog();
  EXPECT_FALSE(LogSink::Instance().empty());
  LogSink::Instance().set_minimum_severity(Severity::GCP_LS_NOTICE);
  GCP_LOG(NOTICE) << "test message";
  LogSink::DisableStdClog();
  EXPECT_TRUE(LogSink::Instance().empty());
  EXPECT_EQ(0U, LogSink::Instance().BackendCount());
  LogSink::Instance().ClearBackends();
}

TEST(LogSinkTest, ClogMultiple) {
  LogSink::EnableStdClog();
  EXPECT_FALSE(LogSink::Instance().empty());
  EXPECT_EQ(1U, LogSink::Instance().BackendCount());
  LogSink::EnableStdClog();
  EXPECT_FALSE(LogSink::Instance().empty());
  EXPECT_EQ(1U, LogSink::Instance().BackendCount());
  LogSink::EnableStdClog();
  EXPECT_FALSE(LogSink::Instance().empty());
  EXPECT_EQ(1U, LogSink::Instance().BackendCount());
  LogSink::Instance().set_minimum_severity(Severity::GCP_LS_NOTICE);
  GCP_LOG(NOTICE) << "test message";
  LogSink::DisableStdClog();
  EXPECT_TRUE(LogSink::Instance().empty());
  EXPECT_EQ(0U, LogSink::Instance().BackendCount());
}


namespace {
/// A class to count calls to IOStream operator.
struct IOStreamCounter {
  int count;
};
std::ostream& operator<<(std::ostream& os, IOStreamCounter& rhs) {
  ++rhs.count;
  return os;
}
}  // namespace

TEST(LogSinkTest, LogCheckCounter) {
  LogSink sink;
  IOStreamCounter counter{0};
  // The following tests could pass if the << operator was a no-op, so for
  // extra paranoia check that this is not the case.
  auto backend = std::make_shared<MockLogBackend>();
  EXPECT_CALL(*backend, ProcessWithOwnership(_)).Times(2);
  sink.AddBackend(backend);
  GOOGLE_CLOUD_CPP_LOG_I(GCP_LS_ALERT, sink) << "count is " << counter;
  GOOGLE_CLOUD_CPP_LOG_I(GCP_LS_FATAL, sink) << "count is " << counter;
  EXPECT_EQ(2, counter.count);
}

TEST(LogSinkTest, LogNoSinks) {
  LogSink sink;
  IOStreamCounter counter{0};
  EXPECT_EQ(0, counter.count);
  GOOGLE_CLOUD_CPP_LOG_I(GCP_LS_WARNING, sink) << "count is " << counter;
  // With no backends, we expect no calls to the iostream operators.
  EXPECT_EQ(0, counter.count);
}

TEST(LogSinkTest, LogDisabledLevels) {
  LogSink sink;
  IOStreamCounter counter{0};
  auto backend = std::make_shared<MockLogBackend>();
  EXPECT_CALL(*backend, ProcessWithOwnership(_)).Times(1);
  sink.AddBackend(backend);

  sink.set_minimum_severity(Severity::GCP_LS_INFO);
  GOOGLE_CLOUD_CPP_LOG_I(GCP_LS_DEBUG, sink) << "count is " << counter;
  // With the DEBUG level disabled we expect no calls.
  EXPECT_EQ(0, counter.count);

  sink.set_minimum_severity(Severity::GCP_LS_ALERT);
  GOOGLE_CLOUD_CPP_LOG_I(GCP_LS_ALERT, sink) << "count is " << counter;
  EXPECT_EQ(1, counter.count);
}

TEST(LogSinkTest, CompileTimeDisabledCannotBeEnabled) {
  LogSink sink;
  IOStreamCounter counter{0};
  auto backend = std::make_shared<MockLogBackend>();
  EXPECT_CALL(*backend, ProcessWithOwnership(_)).Times(1);
  sink.AddBackend(backend);

  // Compile-time disabled logs cannot be enabled at r
  if (Severity::GCP_LS_LOWEST_ENABLED >= Severity::GCP_LS_TRACE) {
    sink.set_minimum_severity(Severity::GCP_LS_TRACE);
    GOOGLE_CLOUD_CPP_LOG_I(GCP_LS_TRACE, sink) << "count is " << counter;
    EXPECT_EQ(0, counter.count);
  }
  sink.set_minimum_severity(Severity::GCP_LS_CRITICAL);
  GOOGLE_CLOUD_CPP_LOG_I(GCP_LS_CRITICAL, sink) << "count is " << counter;
  EXPECT_EQ(1, counter.count);
}

TEST(LogSinkTest, DisabledLogsMakeNoCalls) {
  LogSink sink;

  int counter = 0;
  auto caller = [&counter] { return ++counter; };

  EXPECT_EQ(0, counter);
  GOOGLE_CLOUD_CPP_LOG_I(GCP_LS_WARNING, sink) << "count is " << caller();
  GOOGLE_CLOUD_CPP_LOG_I(GCP_LS_WARNING, sink) << "count is " << caller();
  GOOGLE_CLOUD_CPP_LOG_I(GCP_LS_WARNING, sink) << "count is " << caller();
  GOOGLE_CLOUD_CPP_LOG_I(GCP_LS_WARNING, sink) << "count is " << caller();
  // With no backends, we expect no calls to the expressions in the log line.
  EXPECT_EQ(0, counter);
}
