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
#include "generator/integration_tests/golden/internal/iam_credentials_metadata_decorator.gcpcxx.pb.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace golden_internal {
namespace {

using ::google::cloud::testing_util::IsContextMDValid;
using ::testing::_;

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
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::WriteLogEntriesResponse>,
      WriteLogEntries,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::WriteLogEntriesRequest const
           &request),
      (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::ListLogsResponse>, ListLogs,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::ListLogsRequest const &request),
      (override));
};

class MetadataDecoratorTest : public ::testing::Test {
protected:
  void SetUp() override {
    expected_api_client_header_ = google::cloud::internal::ApiClientHeader();
    mock_ = std::make_shared<MockIAMCredentialsStub>();
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::shared_ptr<MockIAMCredentialsStub> mock_;
  std::string expected_api_client_header_;
};

TEST_F(MetadataDecoratorTest, GenerateAccessToken) {
  EXPECT_CALL(*mock_, GenerateAccessToken(_, _))
      .WillOnce([this](grpc::ClientContext &context,
                       google::test::admin::database::v1::
                           GenerateAccessTokenRequest const &) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context,
            "google.test.admin.database.v1.IAMCredentials.GenerateAccessToken",
            expected_api_client_header_));
        return TransientError();
      });

  IAMCredentialsMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::GenerateAccessTokenRequest request;
  request.set_name("projects/-/serviceAccounts/foo@bar.com");
  auto status = stub.GenerateAccessToken(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, GenerateIdToken) {
  EXPECT_CALL(*mock_, GenerateIdToken(_, _))
      .WillOnce(
          [this](grpc::ClientContext &context,
                 google::test::admin::database::v1::GenerateIdTokenRequest const
                     &) {
            EXPECT_STATUS_OK(IsContextMDValid(
                context,
                "google.test.admin.database.v1.IAMCredentials.GenerateIdToken",
                expected_api_client_header_));
            return TransientError();
          });

  IAMCredentialsMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::GenerateIdTokenRequest request;
  request.set_name("projects/-/serviceAccounts/foo@bar.com");
  auto status = stub.GenerateIdToken(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, WriteLogEntries) {
  EXPECT_CALL(*mock_, WriteLogEntries(_, _))
      .WillOnce(
          [this](grpc::ClientContext &context,
                 google::test::admin::database::v1::WriteLogEntriesRequest const
                     &) {
            EXPECT_STATUS_OK(IsContextMDValid(
                context,
                "google.test.admin.database.v1.IAMCredentials.WriteLogEntries",
                expected_api_client_header_));
            return TransientError();
          });

  IAMCredentialsMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::WriteLogEntriesRequest request;
  auto status = stub.WriteLogEntries(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, ListLogs) {
  EXPECT_CALL(*mock_, ListLogs(_, _))
      .WillOnce(
          [this](grpc::ClientContext &context,
                 google::test::admin::database::v1::ListLogsRequest const &) {
            EXPECT_STATUS_OK(IsContextMDValid(
                context,
                "google.test.admin.database.v1.IAMCredentials.ListLogs",
                expected_api_client_header_));
            return TransientError();
          });

  IAMCredentialsMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListLogsRequest request;
  request.set_parent("projects/my_project");
  auto status = stub.ListLogs(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

} // namespace
} // namespace golden_internal
} // namespace GOOGLE_CLOUD_CPP_NS
} // namespace cloud
} // namespace google
