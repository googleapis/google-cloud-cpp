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

#include "google/cloud/spanner/internal/logging_result_set_reader.h"
#include "google/cloud/spanner/testing/mock_partial_result_set_reader.h"
#include "google/cloud/spanner/tracing_options.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

namespace spanner_proto = ::google::spanner::v1;

class LoggingResultSetReaderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    backend_ =
        std::make_shared<google::cloud::testing_util::CaptureLogLinesBackend>();
    logger_id_ = google::cloud::LogSink::Instance().AddBackend(backend_);
  }

  void TearDown() override {
    google::cloud::LogSink::Instance().RemoveBackend(logger_id_);
    logger_id_ = 0;
  }

  void ClearLogCapture() { backend_->log_lines.clear(); }

  void HasLogLineWith(std::string const& contents) {
    auto count =
        std::count_if(backend_->log_lines.begin(), backend_->log_lines.end(),
                      [&contents](std::string const& line) {
                        return std::string::npos != line.find(contents);
                      });
    EXPECT_NE(0, count);
  }

 private:
  std::shared_ptr<google::cloud::testing_util::CaptureLogLinesBackend> backend_;
  long logger_id_ = 0;  // NOLINT
};

TEST_F(LoggingResultSetReaderTest, TryCancel) {
  auto mock = absl::make_unique<spanner_testing::MockPartialResultSetReader>();
  EXPECT_CALL(*mock, TryCancel()).Times(1);
  LoggingResultSetReader reader(std::move(mock), TracingOptions{});
  reader.TryCancel();

  HasLogLineWith("TryCancel");
}

TEST_F(LoggingResultSetReaderTest, Read) {
  auto mock = absl::make_unique<spanner_testing::MockPartialResultSetReader>();
  EXPECT_CALL(*mock, Read())
      .WillOnce([] {
        spanner_proto::PartialResultSet result;
        result.set_resume_token("test-token");
        return result;
      })
      .WillOnce([] {
        return google::cloud::optional<spanner_proto::PartialResultSet>{};
      });
  LoggingResultSetReader reader(std::move(mock), TracingOptions{});
  auto result = reader.Read();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("test-token", result->resume_token());

  HasLogLineWith("Read");
  HasLogLineWith("test-token");

  // Clear previous captured lines to ensure the following checks for new
  // messages.
  ClearLogCapture();
  result = reader.Read();
  ASSERT_FALSE(result.has_value());
  HasLogLineWith("(optional-with-no-value)");
}

TEST_F(LoggingResultSetReaderTest, Finish) {
  Status const expected_status = Status(StatusCode::kOutOfRange, "weird");
  auto mock = absl::make_unique<spanner_testing::MockPartialResultSetReader>();
  EXPECT_CALL(*mock, Finish()).WillOnce([expected_status] {
    return expected_status;
  });
  LoggingResultSetReader reader(std::move(mock), TracingOptions{});
  auto status = reader.Finish();
  EXPECT_EQ(expected_status, status);

  HasLogLineWith("Finish");
  HasLogLineWith("weird");
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
