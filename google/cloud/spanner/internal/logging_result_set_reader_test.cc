// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
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
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::StartsWith;

class LoggingResultSetReaderTest : public ::testing::Test {
 protected:
  testing_util::ScopedLog log_;
};

TEST_F(LoggingResultSetReaderTest, TryCancel) {
  auto mock = std::make_unique<spanner_testing::MockPartialResultSetReader>();
  EXPECT_CALL(*mock, TryCancel()).Times(1);
  LoggingResultSetReader reader(std::move(mock), TracingOptions{});
  reader.TryCancel();

  EXPECT_THAT(log_.ExtractLines(), IsEmpty());
}

TEST_F(LoggingResultSetReaderTest, Read) {
  auto mock = std::make_unique<spanner_testing::MockPartialResultSetReader>();
  EXPECT_CALL(*mock, Read(_))
      .WillOnce([] {
        google::spanner::v1::PartialResultSet result;
        result.set_resume_token("test-token");
        return PartialResultSet{std::move(result), false};
      })
      .WillOnce([] { return absl::optional<PartialResultSet>{}; });
  LoggingResultSetReader reader(std::move(mock), TracingOptions{});
  auto result = reader.Read("");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("test-token", result->result.resume_token());

  auto log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, AllOf(Contains(StartsWith("Read()"))));
  EXPECT_THAT(log_lines, Contains(HasSubstr("resume_token=\"\"")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("resumption=false")));

  result = reader.Read("test-token");
  ASSERT_FALSE(result.has_value());

  log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, AllOf(Contains(StartsWith("Read()"))));
  EXPECT_THAT(log_lines, Contains(HasSubstr("resume_token=\"test-token\"")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("(optional-with-no-value)")));
}

TEST_F(LoggingResultSetReaderTest, Finish) {
  Status const expected_status = Status(StatusCode::kOutOfRange, "weird");
  auto mock = std::make_unique<spanner_testing::MockPartialResultSetReader>();
  EXPECT_CALL(*mock, Finish()).WillOnce([expected_status] {
    return expected_status;  // NOLINT(performance-no-automatic-move)
  });
  LoggingResultSetReader reader(std::move(mock), TracingOptions{});
  auto status = reader.Finish();
  EXPECT_EQ(expected_status, status);

  auto log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, IsEmpty());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
