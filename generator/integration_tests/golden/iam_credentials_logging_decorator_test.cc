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
#include "generator/integration_tests/golden/internal/iam_credentials_logging_decorator.gcpcxx.pb.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace golden_internal {
namespace {

using ::testing::_;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;

class MockIAMCredentialsStub
    : public google::cloud::golden_internal::IAMCredentialsStub {
public:
  ~MockIAMCredentialsStub() override = default;
  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::GenerateAccessTokenResponse>,
      GenerateAccessToken,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::GenerateAccessTokenRequest const
           &request),
      (override));

  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::GenerateIdTokenResponse>,
      GenerateIdToken,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::GenerateIdTokenRequest const
           &request),
      (override));
};

class LoggingDecoratorTest : public ::testing::Test {
protected:
  void SetUp() override {
    backend_ =
        std::make_shared<google::cloud::testing_util::CaptureLogLinesBackend>();
    logger_id_ = google::cloud::LogSink::Instance().AddBackend(backend_);
    mock_ = std::make_shared<MockIAMCredentialsStub>();
  }

  void TearDown() override {
    google::cloud::LogSink::Instance().RemoveBackend(logger_id_);
    logger_id_ = 0;
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::vector<std::string> ClearLogLines() { return backend_->ClearLogLines(); }

  std::shared_ptr<MockIAMCredentialsStub> mock_;

private:
  std::shared_ptr<google::cloud::testing_util::CaptureLogLinesBackend> backend_;
  long logger_id_ = 0; // NOLINT
};

TEST_F(LoggingDecoratorTest, GenerateAccessToken) {
  ::google::test::admin::database::v1::GenerateAccessTokenResponse response;
  EXPECT_CALL(*mock_, GenerateAccessToken(_, _)).WillOnce(Return(response));
  IAMCredentialsLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.GenerateAccessToken(
      context, google::test::admin::database::v1::GenerateAccessTokenRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GenerateAccessToken")));
}

TEST_F(LoggingDecoratorTest, GenerateAccessTokenError) {
  EXPECT_CALL(*mock_, GenerateAccessToken(_, _))
      .WillOnce(Return(TransientError()));
  IAMCredentialsLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.GenerateAccessToken(
      context, google::test::admin::database::v1::GenerateAccessTokenRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GenerateAccessToken")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, GenerateIdToken) {
  ::google::test::admin::database::v1::GenerateIdTokenResponse response;
  EXPECT_CALL(*mock_, GenerateIdToken(_, _)).WillOnce(Return(response));
  IAMCredentialsLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.GenerateIdToken(
      context, google::test::admin::database::v1::GenerateIdTokenRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GenerateIdToken")));
}

TEST_F(LoggingDecoratorTest, GenerateIdTokenError) {
  EXPECT_CALL(*mock_, GenerateIdToken(_, _)).WillOnce(Return(TransientError()));
  IAMCredentialsLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.GenerateIdToken(
      context, google::test::admin::database::v1::GenerateIdTokenRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GenerateIdToken")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

} // namespace
} // namespace golden_internal
} // namespace GOOGLE_CLOUD_CPP_NS
} // namespace cloud
} // namespace google
