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

#include "google/cloud/logging/internal/logging_service_v2_stub_factory.gcpcxx.pb.h"
#include "google/cloud/logging/logging_service_v2_client.gcpcxx.pb.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace logging {
inline namespace GOOGLE_CLOUD_CPP_GENERATED_NS {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::HasSubstr;

class LoggingIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    connection_options_.enable_tracing("rpc");
    retry_policy_ =
        absl::make_unique<LoggingServiceV2LimitedErrorCountRetryPolicy>(1);
    backoff_policy_ = absl::make_unique<ExponentialBackoffPolicy>(
        std::chrono::seconds(1), std::chrono::seconds(1), 2.0);
  }
  LoggingServiceV2ConnectionOptions connection_options_;
  std::unique_ptr<LoggingServiceV2RetryPolicy> retry_policy_;
  std::unique_ptr<BackoffPolicy> backoff_policy_;
  testing_util::ScopedLog log_;
};

TEST_F(LoggingIntegrationTest, DeleteLogFailure) {
  auto client = LoggingServiceV2Client(
      MakeLoggingServiceV2Connection(connection_options_));
  ::google::logging::v2::DeleteLogRequest request;
  auto response = client.DeleteLog(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteLog")));
}

TEST_F(LoggingIntegrationTest, WriteLogEntries) {
  auto client = LoggingServiceV2Client(
      MakeLoggingServiceV2Connection(connection_options_));
  ::google::logging::v2::WriteLogEntriesRequest request;
  auto response = client.WriteLogEntries(request);
  EXPECT_THAT(response, IsOk());
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("WriteLogEntries")));
}

TEST_F(LoggingIntegrationTest, ListLogEntriesFailure) {
  connection_options_.set_endpoint("localhost:1");
  auto client = LoggingServiceV2Client(MakeLoggingServiceV2Connection(
      connection_options_, retry_policy_->clone(), backoff_policy_->clone(),
      MakeDefaultLoggingServiceV2ConnectionIdempotencyPolicy()));
  ::google::logging::v2::ListLogEntriesRequest request;
  auto range = client.ListLogEntries(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kUnavailable));
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListLogEntries")));
}

TEST_F(LoggingIntegrationTest, ListMonitoredResourceDescriptors) {
  auto client = LoggingServiceV2Client(MakeLoggingServiceV2Connection(
      connection_options_, retry_policy_->clone(), backoff_policy_->clone(),
      MakeDefaultLoggingServiceV2ConnectionIdempotencyPolicy()));
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
  auto client = LoggingServiceV2Client(MakeLoggingServiceV2Connection(
      connection_options_, retry_policy_->clone(), backoff_policy_->clone(),
      MakeDefaultLoggingServiceV2ConnectionIdempotencyPolicy()));
  ::google::logging::v2::ListLogsRequest request;
  auto range = client.ListLogs(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kInvalidArgument));
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListLogs")));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace logging
}  // namespace cloud
}  // namespace google
