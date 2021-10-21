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

#include "generator/integration_tests/golden/internal/golden_kitchen_sink_metadata_decorator.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "absl/memory/memory.h"
#include "generator/integration_tests/golden/mocks/mock_golden_kitchen_sink_stub.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
inline namespace GOOGLE_CLOUD_CPP_GENERATED_NS {
namespace {

using ::google::cloud::golden_internal::MockGoldenKitchenSinkStub;
using ::google::cloud::golden_internal::MockTailLogEntriesStreamingReadRpc;
using ::google::cloud::testing_util::IsContextMDValid;
using ::google::cloud::testing_util::IsOk;
using ::google::test::admin::database::v1::TailLogEntriesRequest;
using ::google::test::admin::database::v1::TailLogEntriesResponse;
using ::testing::Not;
using ::testing::Return;

class MetadataDecoratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    expected_api_client_header_ = google::cloud::internal::ApiClientHeader();
    mock_ = std::make_shared<MockGoldenKitchenSinkStub>();
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::shared_ptr<MockGoldenKitchenSinkStub> mock_;
  std::string expected_api_client_header_;
};

TEST_F(MetadataDecoratorTest, GenerateAccessToken) {
  EXPECT_CALL(*mock_, GenerateAccessToken)
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           GenerateAccessTokenRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.test.admin.database.v1.GoldenKitchenSink."
                             "GenerateAccessToken",
                             expected_api_client_header_));
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::GenerateAccessTokenRequest request;
  request.set_name("projects/-/serviceAccounts/foo@bar.com");
  auto status = stub.GenerateAccessToken(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, GenerateIdToken) {
  EXPECT_CALL(*mock_, GenerateIdToken)
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           GenerateIdTokenRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context,
            "google.test.admin.database.v1.GoldenKitchenSink.GenerateIdToken",
            expected_api_client_header_));
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::GenerateIdTokenRequest request;
  request.set_name("projects/-/serviceAccounts/foo@bar.com");
  auto status = stub.GenerateIdToken(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, WriteLogEntries) {
  EXPECT_CALL(*mock_, WriteLogEntries)
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           WriteLogEntriesRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context,
            "google.test.admin.database.v1.GoldenKitchenSink.WriteLogEntries",
            expected_api_client_header_));
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::WriteLogEntriesRequest request;
  auto status = stub.WriteLogEntries(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, ListLogs) {
  EXPECT_CALL(*mock_, ListLogs)
      .WillOnce([this](
                    grpc::ClientContext& context,
                    google::test::admin::database::v1::ListLogsRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context, "google.test.admin.database.v1.GoldenKitchenSink.ListLogs",
            expected_api_client_header_));
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListLogsRequest request;
  request.set_parent("projects/my_project");
  auto status = stub.ListLogs(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, TailLogEntries) {
  EXPECT_CALL(*mock_, TailLogEntries)
      .WillOnce([this](std::unique_ptr<grpc::ClientContext> context,
                       TailLogEntriesRequest const&) {
        auto mock_response =
            absl::make_unique<MockTailLogEntriesStreamingReadRpc>();
        EXPECT_CALL(*mock_response, Read)
            .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.test.admin.database.v1.GoldenKitchenSink.TailLogEntries",
            expected_api_client_header_));
        return std::unique_ptr<
            internal::StreamingReadRpc<TailLogEntriesResponse>>(
            std::move(mock_response));
      });
  GoldenKitchenSinkMetadata stub(mock_);
  TailLogEntriesRequest request;
  auto response =
      stub.TailLogEntries(absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(absl::get<Status>(response->Read()), Not(IsOk()));
}

TEST_F(MetadataDecoratorTest, ListServiceAccountKeys) {
  EXPECT_CALL(*mock_, ListServiceAccountKeys)
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           ListServiceAccountKeysRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.test.admin.database.v1.GoldenKitchenSink."
                             "ListServiceAccountKeys",
                             expected_api_client_header_));
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListServiceAccountKeysRequest request;
  request.set_name("projects/my-project/serviceAccounts/foo@bar.com");
  auto status = stub.ListServiceAccountKeys(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
