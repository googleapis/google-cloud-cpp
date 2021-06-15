// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/logging/internal/logging_service_v2_stub_factory.h"
#include "google/cloud/logging/logging_service_v2_client.h"
#include "google/cloud/logging/logging_service_v2_options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace logging {
inline namespace GOOGLE_CLOUD_CPP_GENERATED_NS {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::testing::Contains;
using ::testing::HasSubstr;

class LoggingIntegrationTest
    : public ::google::cloud::testing_util::IntegrationTest {
 protected:
  testing_util::ScopedLog log_;
};

Options TestFailureOptions() {
  auto const expiration =
      std::chrono::system_clock::now() + std::chrono::minutes(15);
  return Options{}
      .set<TracingComponentsOption>({"rpc"})
      .set<UnifiedCredentialsOption>(
          MakeAccessTokenCredentials("invalid-access-token", expiration))
      .set<LoggingServiceV2RetryPolicyOption>(
          LoggingServiceV2LimitedErrorCountRetryPolicy(1).clone())
      .set<LoggingServiceV2BackoffPolicyOption>(
          ExponentialBackoffPolicy(std::chrono::seconds(1),
                                   std::chrono::seconds(1), 2.0)
              .clone());
}

Options TestSuccessOptions() {
  return Options{}.set<TracingComponentsOption>({"rpc"});
}

TEST_F(LoggingIntegrationTest, DeleteLogFailure) {
  auto client = LoggingServiceV2Client(
      MakeLoggingServiceV2Connection(TestFailureOptions()));
  ::google::logging::v2::DeleteLogRequest request;
  auto response = client.DeleteLog(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteLog")));
}

TEST_F(LoggingIntegrationTest, WriteLogEntries) {
  auto client = LoggingServiceV2Client(
      MakeLoggingServiceV2Connection(TestSuccessOptions()));
  ::google::logging::v2::WriteLogEntriesRequest request;
  auto response = client.WriteLogEntries(request);
  EXPECT_THAT(response, IsOk());
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("WriteLogEntries")));
}

TEST_F(LoggingIntegrationTest, ListLogEntriesFailure) {
  auto client = LoggingServiceV2Client(
      MakeLoggingServiceV2Connection(TestFailureOptions()));
  ::google::logging::v2::ListLogEntriesRequest request;
  auto range = client.ListLogEntries(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, Not(IsOk()));
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListLogEntries")));
}

TEST_F(LoggingIntegrationTest, ListMonitoredResourceDescriptors) {
  auto client = LoggingServiceV2Client(
      MakeLoggingServiceV2Connection(TestSuccessOptions()));
  ::google::logging::v2::ListMonitoredResourceDescriptorsRequest request;
  auto range = client.ListMonitoredResourceDescriptors(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, IsOk());
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(HasSubstr("ListMonitoredResourceDescriptors")));
}

TEST_F(LoggingIntegrationTest, ListLogsFailure) {
  auto client = LoggingServiceV2Client(
      MakeLoggingServiceV2Connection(TestFailureOptions()));
  ::google::logging::v2::ListLogsRequest request;
  auto range = client.ListLogs(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, Not(IsOk()));
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListLogs")));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace logging
}  // namespace cloud
}  // namespace google
