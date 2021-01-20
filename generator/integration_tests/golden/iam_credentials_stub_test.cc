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

#include "generator/integration_tests/golden/internal/iam_credentials_stub.gcpcxx.pb.h"
#include <gmock/gmock.h>
#include <memory>

using ::testing::_;
using ::testing::Return;

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace golden_internal {
namespace {
class MockGrpcIAMCredentialsStub : public ::google::test::admin::database::v1::
                                       IAMCredentials::StubInterface {
public:
  ~MockGrpcIAMCredentialsStub() override = default;
  MOCK_METHOD(
      ::grpc::Status, GenerateAccessToken,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::GenerateAccessTokenRequest
           &request,
       ::google::test::admin::database::v1::GenerateAccessTokenResponse
           *response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, GenerateIdToken,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::GenerateIdTokenRequest
           &request,
       ::google::test::admin::database::v1::GenerateIdTokenResponse *response),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::GenerateAccessTokenResponse> *,
      AsyncGenerateAccessTokenRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::GenerateAccessTokenRequest
           &request,
       ::grpc::CompletionQueue *cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::GenerateAccessTokenResponse> *,
      PrepareAsyncGenerateAccessTokenRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::GenerateAccessTokenRequest
           &request,
       ::grpc::CompletionQueue *cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::GenerateIdTokenResponse> *,
      AsyncGenerateIdTokenRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::GenerateIdTokenRequest
           &request,
       ::grpc::CompletionQueue *cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::GenerateIdTokenResponse> *,
      PrepareAsyncGenerateIdTokenRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::GenerateIdTokenRequest
           &request,
       ::grpc::CompletionQueue *cq),
      (override));
  MOCK_METHOD(
      ::grpc::Status, WriteLogEntries,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::WriteLogEntriesRequest
           &request,
       ::google::test::admin::database::v1::WriteLogEntriesResponse *response),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::WriteLogEntriesResponse> *,
      AsyncWriteLogEntriesRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::WriteLogEntriesRequest
           &request,
       ::grpc::CompletionQueue *cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::WriteLogEntriesResponse> *,
      PrepareAsyncWriteLogEntriesRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::WriteLogEntriesRequest
           &request,
       ::grpc::CompletionQueue *cq),
      (override));
  MOCK_METHOD(
      ::grpc::Status, ListLogs,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListLogsRequest &request,
       ::google::test::admin::database::v1::ListLogsResponse *response),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListLogsResponse> *,
      AsyncListLogsRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListLogsRequest &request,
       ::grpc::CompletionQueue *cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListLogsResponse> *,
      PrepareAsyncListLogsRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListLogsRequest &request,
       ::grpc::CompletionQueue *cq),
      (override));
};

class IAMCredentialsStubTest : public ::testing::Test {
protected:
  void SetUp() override {
    grpc_stub_ = std::unique_ptr<MockGrpcIAMCredentialsStub>(
        new MockGrpcIAMCredentialsStub());
  }

  static grpc::Status GrpcTransientError() {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  }
  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::unique_ptr<MockGrpcIAMCredentialsStub> grpc_stub_;
};

TEST_F(IAMCredentialsStubTest, GenerateAccessToken) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::GenerateAccessTokenRequest request;
  EXPECT_CALL(*grpc_stub_, GenerateAccessToken(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultIAMCredentialsStub stub(std::move(grpc_stub_));
  auto success = stub.GenerateAccessToken(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.GenerateAccessToken(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(IAMCredentialsStubTest, GenerateIdToken) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::GenerateIdTokenRequest request;
  EXPECT_CALL(*grpc_stub_, GenerateIdToken(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultIAMCredentialsStub stub(std::move(grpc_stub_));
  auto success = stub.GenerateIdToken(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.GenerateIdToken(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(IAMCredentialsStubTest, WriteLogEntries) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::WriteLogEntriesRequest request;
  EXPECT_CALL(*grpc_stub_, WriteLogEntries(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultIAMCredentialsStub stub(std::move(grpc_stub_));
  auto success = stub.WriteLogEntries(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.WriteLogEntries(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(IAMCredentialsStubTest, ListLogs) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::ListLogsRequest request;
  EXPECT_CALL(*grpc_stub_, ListLogs(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultIAMCredentialsStub stub(std::move(grpc_stub_));
  auto success = stub.ListLogs(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.ListLogs(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

} // namespace
} // namespace golden_internal
} // namespace GOOGLE_CLOUD_CPP_NS
} // namespace cloud
} // namespace google
